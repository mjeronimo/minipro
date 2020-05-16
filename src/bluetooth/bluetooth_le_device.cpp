// Copyright (c) 2020 Michael Jeronimo
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "bluetooth/bluetooth_le_device.hpp"

#include <stdio.h>
#include <unistd.h>
#include <getopt.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include "bluez.h"
#include "minipro/minipro.hpp"
#include "util/joystick.hpp"
#include "bluetooth/utils.hpp"

using namespace std::chrono_literals;
using namespace jeronibot::util;

namespace bluetooth
{

// class BluetoothLEClient
BluetoothLEDevice::BluetoothLEDevice(
  const std::string & device_address, uint8_t dst_type, int sec,
  uint16_t mtu)
{
  bdaddr_t dst_addr;
  str2ba(device_address.c_str(), &dst_addr);

  bdaddr_t src_addr;
  bdaddr_t bdaddr_any = {{0, 0, 0, 0, 0, 0}};
  bacpy(&src_addr, &bdaddr_any);

  mainloop_init();

  // class bluetooth::L2CapSocket

  fd_ = l2cap_le_att_connect(&src_addr, &dst_addr, dst_type, sec);
  if (fd_ < 0) {
    throw std::runtime_error("BluetoothLEDevice: Failed to connect to Bluetooth device");
  }

  att_ = bt_att_new(fd_, false);
  if (!att_) {
    fprintf(stderr, "Failed to initialize ATT transport layer\n");
    bt_att_unref(att_);
    return;
  }

  if (!bt_att_set_close_on_unref(att_, true)) {
    fprintf(stderr, "Failed to set up ATT transport layer\n");
    bt_att_unref(att_);
    return;
  }

  if (!bt_att_register_disconnect(
      att_, BluetoothLEDevice::att_disconnect_cb, nullptr,
      nullptr))
  {
    fprintf(stderr, "Failed to set ATT disconnect handler\n");
    bt_att_unref(att_);
    return;
  }

  // class bluetooth GattClient
  db_ = gatt_db_new();
  if (!db_) {
    fprintf(stderr, "Failed to create GATT database\n");
    bt_att_unref(att_);
    return;
  }

  gatt_ = bt_gatt_client_new(db_, att_, mtu);
  if (!gatt_) {
    fprintf(stderr, "Failed to create GATT client\n");
    gatt_db_unref(db_);
    bt_att_unref(att_);
    return;
  }

  gatt_db_register(db_, service_added_cb, service_removed_cb, nullptr, nullptr);

  bt_gatt_client_set_ready_handler(gatt_, ready_cb, this, nullptr);
  bt_gatt_client_set_service_changed(gatt_, service_changed_cb, this, nullptr);

  // bt_gatt_client already holds a reference
  gatt_db_unref(db_);

  // bt_gatt_client_set_ready_handler(gatt_, ready_cb, this, nullptr);
  // bt_gatt_client_set_service_changed(gatt_, service_changed_cb, this, nullptr);

  input_thread_ = std::make_unique<std::thread>(std::bind(&BluetoothLEDevice::process_input, this));

  // Wait for client to be ready
  std::unique_lock<std::mutex> lk(mutex_);
  if (cv_.wait_for(lk, 5s, [this] {return ready_;})) {
    printf("BluetoothLEDevice: Initialized OK\n");
  } else {
    throw std::runtime_error("BluetoothLEDevice: Did NOT initialize OK");
  }
}

BluetoothLEDevice::~BluetoothLEDevice()
{
  printf("BluetoothLEDevice::~BluetoothLEDevice\n");
  mainloop_quit();
  printf("BluetoothLEDevice::~BluetoothLEDevice: after calling mainloop_quit\n");
  input_thread_->join();
  printf("BluetoothLEDevice: Shutting down...\n");

  // client_destroy();
  bt_gatt_client_unref(gatt_);
  bt_att_unref(att_);

  printf("BluetoothLEDevice::~BluetoothLEDevice: after client_destroy\n");
}

void
BluetoothLEDevice::att_disconnect_cb(int err, void * /*user_data*/)
{
  printf("Device disconnected: %s\n", strerror(err));
  mainloop_quit();
}

void
BluetoothLEDevice::service_added_cb(struct gatt_db_attribute * attr, void * user_data)
{
}

void
BluetoothLEDevice::service_removed_cb(struct gatt_db_attribute * attr, void * user_data)
{
}

void
BluetoothLEDevice::print_uuid(const bt_uuid_t * uuid)
{
  char uuid_str[MAX_LEN_UUID_STR];
  bt_uuid_t uuid128;

  bt_uuid_to_uuid128(uuid, &uuid128);
  bt_uuid_to_string(&uuid128, uuid_str, sizeof(uuid_str));

  printf("%s\n", uuid_str);
}

void
BluetoothLEDevice::print_included_data(struct gatt_db_attribute * attr, void * user_data)
{
  BluetoothLEDevice * This = (BluetoothLEDevice *) user_data;

  uint16_t handle;
  uint16_t start;
  uint16_t end;

  if (!gatt_db_attribute_get_incl_data(attr, &handle, &start, &end)) {
    return;
  }

  struct gatt_db_attribute * service = gatt_db_get_attribute(This->db_, start);
  if (!service) {
    return;
  }

  bt_uuid_t uuid;
  gatt_db_attribute_get_service_uuid(service, &uuid);

  printf("\t  include - handle: 0x%04x, - start: 0x%04x, end: 0x%04x, uuid: ", handle, start, end);
  BluetoothLEDevice::print_uuid(&uuid);
}

void
BluetoothLEDevice::print_descriptor(struct gatt_db_attribute * attr, void * user_data)
{
  printf("\t\t  descr - handle: 0x%04x, uuid: ", gatt_db_attribute_get_handle(attr));
  BluetoothLEDevice::print_uuid(gatt_db_attribute_get_type(attr));
}

void
BluetoothLEDevice::print_characteristic(struct gatt_db_attribute * attr, void * user_data)
{
  uint16_t handle;
  uint16_t value_handle;
  uint8_t properties;
  bt_uuid_t uuid;

  if (!gatt_db_attribute_get_char_data(attr, &handle, &value_handle, &properties, &uuid))
  {
    return;
  }

  printf("\t  charac - start: 0x%04x, value: 0x%04x, " "props: 0x%02x, uuid: ", handle, value_handle, properties);
  BluetoothLEDevice::print_uuid(&uuid);

  gatt_db_service_foreach_desc(attr, print_descriptor, nullptr);
}

void
BluetoothLEDevice::print_service(struct gatt_db_attribute * attr, void * user_data)
{
  BluetoothLEDevice *This = (BluetoothLEDevice *) user_data;

  uint16_t start, end;
  bool primary;
  bt_uuid_t uuid;

  if (!gatt_db_attribute_get_service_data(attr, &start, &end, &primary, &uuid)) {
    return;
  }

  printf("service - start: 0x%04x, end: 0x%04x, type: %s, uuid: ", start, end, primary ? "primary" : "secondary");
  BluetoothLEDevice::print_uuid(&uuid);

  gatt_db_service_foreach_incl(attr, print_included_data, This);
  gatt_db_service_foreach_char(attr, print_characteristic, nullptr);

  printf("\n");
}

void
BluetoothLEDevice::ready_cb(bool success, uint8_t att_ecode, void * user_data)
{
  BluetoothLEDevice * This = (BluetoothLEDevice *) user_data;

  if (!success) {
    printf("GATT discovery procedures failed - error code: 0x%02x\n", att_ecode);
    return;
  }

  {
    std::lock_guard<std::mutex> lk(This->mutex_);
    This->ready_ = true;
  }

  This->cv_.notify_all();
  This->ready_ = true;
}

void
BluetoothLEDevice::service_changed_cb(uint16_t start_handle, uint16_t end_handle, void * user_data)
{
  BluetoothLEDevice * This = (BluetoothLEDevice *) user_data;

  printf("\nService Changed handled - start: 0x%04x end: 0x%04x\n", start_handle, end_handle);
  gatt_db_foreach_service_in_range(This->db_, nullptr, print_service, This, start_handle, end_handle);
}

/**
 * parse command string
 *
 * @param str      command to parse
 * @param expected_argc  number of arguments expected
 * @param argv      pointers to arguments separated by space or tab
 * @param argc      argument counter (and actual count)
 * @return        true = success false = error
 */
bool
BluetoothLEDevice::parse_args(char * str, int expected_argc, char ** argv, int * argc)
{
  for (char ** ap = argv; (*ap = strsep(&str, " \t")) != nullptr; ) {
    if (**ap == '\0') {
      continue;
    }

    (*argc)++;
    ap++;

    if (*argc > expected_argc) {
      return false;
    }
  }

  return true;
}

void
BluetoothLEDevice::read_multiple_cb(
  bool success, uint8_t att_ecode, const uint8_t * value, uint16_t length, void * /*user_data*/)
{
  if (!success) {
    printf("\nRead multiple request failed: 0x%02x\n", att_ecode);
    return;
  }

  printf("\nRead multiple value (%u bytes):", length);
  for (int i = 0; i < length; i++) {
    printf("%02x ", value[i]);
  }
  printf("\n");
}

void
BluetoothLEDevice::read_multiple(uint16_t * handles, uint8_t num_handles)
{
  if (!bt_gatt_client_is_ready(gatt_)) {
    printf("GATT client not initialized\n");
    return;
  }

  if (!bt_gatt_client_read_multiple(gatt_, handles, num_handles, read_multiple_cb, nullptr, nullptr)) {
    printf("Failed to initiate read multiple procedure\n");
  }
}

void
BluetoothLEDevice::read_cb(bool success, uint8_t att_ecode, const uint8_t * value, uint16_t length, void * /*user_data*/)
{
  if (!success) {
    printf(
      "\nRead request failed: %s (0x%02x)\n",
      bluetooth::utils::to_string(att_ecode), att_ecode);
    return;
  }

  printf("\nRead value");
  if (length == 0) {
    printf(": 0 bytes\n");
    return;
  }

  printf(" (%u bytes): ", length);
  for (int i = 0; i < length; i++) {
    printf("%02x ", value[i]);
  }

  printf("\n");
}

void
BluetoothLEDevice::read_value(uint16_t handle)
{
  if (!bt_gatt_client_is_ready(gatt_)) {
    printf("GATT client not initialized\n");
    return;
  }

  if (!bt_gatt_client_read_value(gatt_, handle, read_cb, nullptr, nullptr)) {
    printf("Failed to initiate read value\n");
  }
}

void
BluetoothLEDevice::read_long_value(uint16_t handle, uint16_t offset)
{
  if (!bt_gatt_client_is_ready(gatt_)) {
    printf("GATT client not initialized\n");
    return;
  }

  if (!bt_gatt_client_read_long_value(gatt_, handle, offset, read_cb, nullptr, nullptr)) {
    printf("Failed to initiate read long value\n");
  }
}

static struct option write_value_options[] = {
  {"without-response", 0, 0, 'w'},
  {"signed-write", 0, 0, 's'},
  {}
};

void
BluetoothLEDevice::write_cb(bool success, uint8_t att_ecode, void * user_data)
{
  if (!success) {
    printf("Write failed: %s (0x%02x)\n", bluetooth::utils::to_string(att_ecode), att_ecode);
  }
}

static struct option write_long_value_options[] = {
  {"reliable-write", 0, 0, 'r'},
  {}
};

void
BluetoothLEDevice::write_long_cb(bool success, bool reliable_error, uint8_t att_ecode, void * /*user_data*/)
{
  if (success) {
    // printf("Write successful\n");
  } else if (reliable_error) {
    printf("Reliable write not verified\n");
  } else {
    printf("Write failed: %s (0x%02x)\n", bluetooth::utils::to_string(att_ecode), att_ecode);
  }
}

void
BluetoothLEDevice::write_long_value(
      bool reliable_writes, uint16_t handle,
      uint16_t offset, uint8_t * value, int length)
{
  if (!bt_gatt_client_is_ready(gatt_)) {
    printf("GATT client not initialized\n");
    return;
  }

  if (!bt_gatt_client_write_long_value(gatt_, reliable_writes, handle,
      offset, value, length, write_long_cb, nullptr, nullptr))
  {
    printf("Failed to initiate long write procedure\n");
  }
}

static struct option write_prepare_options[] = {
  {"session-id", 1, 0, 's'},
  {}
};

void
BluetoothLEDevice::write_prepare(unsigned int id, uint16_t handle, uint16_t offset, uint8_t * value, unsigned int length)
{
  if (!bt_gatt_client_is_ready(gatt_)) {
    printf("GATT client not initialized\n");
    return;
  }

  if (reliable_session_id_ != id) {
    printf("Session id != Ongoing session id (%u!=%u)\n", id, reliable_session_id_);
    return;
  }

  reliable_session_id_ = bt_gatt_client_prepare_write(gatt_, id, handle, offset, value, length,
      write_long_cb, nullptr, nullptr);

  if (!reliable_session_id_) {
    printf("Failed to proceed prepare write\n");
  } else {
    printf("Prepare write success.\nSession id: %d to be used on next write\n", reliable_session_id_);
  }
}

void
BluetoothLEDevice::write_execute(unsigned int session_id, bool execute)
{
  if (!bt_gatt_client_is_ready(gatt_)) {
    printf("GATT client not initialized\n");
    return;
  }

  if (execute) {
    if (!bt_gatt_client_write_execute(gatt_, session_id, write_cb, nullptr, nullptr)) {
      printf("Failed to proceed write execute\n");
    }
  } else {
    bt_gatt_client_cancel(gatt_, session_id);
  }

  reliable_session_id_ = 0;
}

void
BluetoothLEDevice::notify_cb(
  uint16_t value_handle, const uint8_t * value,
  uint16_t length, void * user_data)
{
  printf("\n\tHandle Value Not/Ind: 0x%04x - ", value_handle);

  if (length == 0) {
    printf("(0 bytes)\n");
    return;
  }

  printf("(%u bytes): ", length);

  for (int i = 0; i < length; i++) {
    printf("%02x ", value[i]);
  }

  printf("\n");
}

void
BluetoothLEDevice::register_notify_cb(uint16_t att_ecode, void * /*user_data*/)
{
  if (att_ecode) {
    printf("Failed to register notify handler - error code: 0x%02x\n", att_ecode);
    return;
  }

  printf("Registered notify handler!");
}

// TODO(mjeronimo): return unsigned int?
void
BluetoothLEDevice::register_notify(uint16_t value_handle)
{
  if (!bt_gatt_client_is_ready(gatt_)) {
    printf("GATT client not initialized\n");
    return;
  }

  unsigned int id = bt_gatt_client_register_notify(
    gatt_, value_handle, register_notify_cb, notify_cb, nullptr, nullptr);

  if (!id) {
    printf("Failed to register notify handler\n");
    return;
  }
}

void
BluetoothLEDevice::unregister_notify(unsigned int id)
{
  if (!bt_gatt_client_is_ready(gatt_)) {
    printf("GATT client not initialized\n");
    return;
  }

  if (!bt_gatt_client_unregister_notify(gatt_, id)) {
    printf("Failed to unregister notify handler with id: %u\n", id);
  }
}

void
BluetoothLEDevice::set_security(int level)
{
  if (!bt_gatt_client_is_ready(gatt_)) {
    printf("GATT client not initialized\n");
    return;
  }

  if (level < 1 || level > 3) {
    printf("Invalid level: %d\n", level);
    return;
  }

  if (!bt_gatt_client_set_security(gatt_, level)) {
    printf("Could not set security level\n");
  }
}

#if 0
3 #define BT_SECURITY 4
   struct bt_security {
       uint8_t level;
       uint8_t key_size;
   };
   #define BT_SECURITY_SDP     0
   #define BT_SECURITY_LOW     1
   #define BT_SECURITY_MEDIUM  2
   #define BT_SECURITY_HIGH    3
#endif
int
BluetoothLEDevice::get_security()
{
  if (!bt_gatt_client_is_ready(gatt_)) {
    printf("GATT client not initialized\n");
    return -1;
  }

  return bt_gatt_client_get_security(gatt_);
}

bool
BluetoothLEDevice::local_counter(uint32_t * sign_cnt, void * /*user_data*/)
{
  static uint32_t cnt = 0;
  *sign_cnt = cnt++;
  return true;
}

void
BluetoothLEDevice::set_sign_key(uint8_t key[16])
{
  bt_att_set_local_key(att_, key, local_counter, this);
}

/**
 * create a bluetooth le l2cap socket and connect src to dst
 *
 * @param src  6 bytes BD Address source address
 * @param dst  6 bytes BD Address destination address
 * @param dst_type  destination type BDADDR_LE_PUBLIC or BDADDR_LE_RANDOM
 * @param sec   security level BT_SECURITY_LOW or BT_SECURITY_MEDIUM or BT_SECURITY_HIGH
 * @return socket or -1 if error
 */

class BluetoothL2Cap2Socket
{
};

int
BluetoothLEDevice::l2cap_le_att_connect(bdaddr_t * src, bdaddr_t * dst, uint8_t dst_type, int sec)
{
  struct bt_security btsec;

  char dstaddr_str[18];
  ba2str(dst, dstaddr_str);

  if (false) {
    char srcaddr_str[18];
    ba2str(src, srcaddr_str);

    printf(
      "btgatt-client: Opening L2CAP LE connection on ATT "
      "channel:\n\t src: %s\n\tdest: %s\n",
      srcaddr_str, dstaddr_str);
  }

  int sock = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
  if (sock < 0) {
    perror("BluetoothLEDevice: Failed to create L2CAP socket");
    return -1;
  }

  #define ATT_CID 4

  // Set up source address
  struct sockaddr_l2 srcaddr;
  memset(&srcaddr, 0, sizeof(srcaddr));
  srcaddr.l2_family = AF_BLUETOOTH;
  srcaddr.l2_cid = htobs(ATT_CID);
  srcaddr.l2_bdaddr_type = 0;
  bacpy(&srcaddr.l2_bdaddr, src);

  if (bind(sock, (struct sockaddr *)&srcaddr, sizeof(srcaddr)) < 0) {
    perror("BluetoothLEDevice: Failed to bind L2CAP socket");
    close(sock);
    return -1;
  }

  // Set the security level
  struct sockaddr_l2 dstaddr;
  memset(&btsec, 0, sizeof(btsec));
  btsec.level = sec;
  if (setsockopt(sock, SOL_BLUETOOTH, BT_SECURITY, &btsec, sizeof(btsec)) != 0) {
    fprintf(stderr, "BluetoothLEDevice: Failed to set L2CAP security level\n");
    close(sock);
    return -1;
  }

  // Set up destination address
  memset(&dstaddr, 0, sizeof(dstaddr));
  dstaddr.l2_family = AF_BLUETOOTH;
  dstaddr.l2_cid = htobs(ATT_CID);
  dstaddr.l2_bdaddr_type = dst_type;
  bacpy(&dstaddr.l2_bdaddr, dst);

  printf("BluetoothLEDevice: Connecting to device (%s)...\n", dstaddr_str);
  fflush(stdout);

  if (connect(sock, (struct sockaddr *) &dstaddr, sizeof(dstaddr)) < 0) {
    printf("BluetoothLEDevice: Failed to connect to device\n");
    close(sock);
    return -1;
  }

  printf("BluetoothLEDevice: Connected\n");
  return sock;
}

void
BluetoothLEDevice::write_value(char * cmd_str)
{
  int opt;
  char * argvbuf[516];
  char ** argv = argvbuf;
  int argc = 1;
  char * endptr = nullptr;
  uint8_t * value = nullptr;

  // printf("cmd_write_value: cmd_str: \"%s\"\n", cmd_str);

  if (!bt_gatt_client_is_ready(gatt_)) {
    printf("GATT client not initialized\n");
    return;
  }

  if (!parse_args(cmd_str, 514, argv + 1, &argc)) {
    printf("Too many arguments\n");
    return;
  }

  bool without_response = false;
  bool signed_write = false;

  optind = 0;
  argv[0] = (char *) "write-value";
  while ((opt = getopt_long(argc, argv, "+ws", write_value_options, nullptr)) != -1) {
    switch (opt) {
      case 'w':
        without_response = true;
        break;
      case 's':
        signed_write = true;
        break;
      default:
        return;
    }
  }

  argc -= optind;
  argv += optind;

  if (argc < 1) {
    return;
  }

  uint16_t handle = strtol(argv[0], &endptr, 0);
  if (!endptr || *endptr != '\0' || !handle) {
    printf("Invalid handle: %s\n", argv[0]);
    return;
  }

  int length = argc - 1;
  if (length > 0) {
    if (length > UINT16_MAX) {
      printf("Write value too long\n");
      return;
    }

    value = reinterpret_cast<uint8_t *>(malloc(length));
    if (!value) {
      printf("Failed to construct write value\n");
      return;
    }

    for (int i = 1; i < argc; i++) {
      value[i - 1] = strtol(argv[i], &endptr, 16);
      if (endptr == argv[i] || *endptr != '\0' ||
        errno == ERANGE)
      {
        printf(
          "Invalid value byte: %s\n",
          argv[i]);
        goto done;
      }
    }
  }

  if (without_response) {
    if (!bt_gatt_client_write_without_response(gatt_, handle, signed_write, value, length)) {
      printf("Failed to initiate write without response procedure\n");
      goto done;
    }
    goto done;
  }

  if (!bt_gatt_client_write_value(gatt_, handle, value, length, write_cb, nullptr, nullptr)) {
    printf("Failed to initiate write procedure\n");
  }

done:
  free(value);
}

void
BluetoothLEDevice::process_input()
{
  printf("BluetoothLEDevice::process_input: calling mainloop_run");
  mainloop_run();
  printf("BluetoothLEDevice::process_input: aftercalling mainloop_run");
}

}  // namespace bluetooth
