// Copyright (c) 2020 Michael Jeronimo
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, softwars
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "bluetooth/bluetooth_le_client.hpp"

#include <unistd.h>

#include <chrono>
#include <cstdio>
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

BluetoothLEClient::BluetoothLEClient(const std::string & device_address, uint8_t dst_type, int sec, uint16_t mtu)
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
    throw std::runtime_error("BluetoothLEClient: Failed to connect to Bluetooth device");
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

  if (!bt_att_register_disconnect(att_, BluetoothLEClient::att_disconnect_cb, nullptr, nullptr)) {
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

  input_thread_ = std::make_unique<std::thread>(std::bind(&BluetoothLEClient::process_input, this));

  // Wait for client to be ready
  std::unique_lock<std::mutex> lk(mutex_);
  if (cv_.wait_for(lk, 5s, [this] {return ready_;})) {
    printf("BluetoothLEClient: Initialized OK\n");
  } else {
    throw std::runtime_error("BluetoothLEClient: Did NOT initialize OK");
  }
}

BluetoothLEClient::~BluetoothLEClient()
{
  mainloop_quit();
  input_thread_->join();
  bt_gatt_client_unref(gatt_);
  bt_att_unref(att_);
}

void
BluetoothLEClient::att_disconnect_cb(int err, void * /*user_data*/)
{
  printf("Device disconnected: %s\n", strerror(err));
  mainloop_quit();
}

void
BluetoothLEClient::service_added_cb(struct gatt_db_attribute * attr, void * user_data)
{
}

void
BluetoothLEClient::service_removed_cb(struct gatt_db_attribute * attr, void * user_data)
{
}

void
BluetoothLEClient::print_uuid(const bt_uuid_t * uuid)
{
  char uuid_str[MAX_LEN_UUID_STR];
  bt_uuid_t uuid128;

  bt_uuid_to_uuid128(uuid, &uuid128);
  bt_uuid_to_string(&uuid128, uuid_str, sizeof(uuid_str));

  printf("%s\n", uuid_str);
}

void
BluetoothLEClient::print_included_data(struct gatt_db_attribute * attr, void * user_data)
{
  BluetoothLEClient * This = (BluetoothLEClient *) user_data;

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
  BluetoothLEClient::print_uuid(&uuid);
}

void
BluetoothLEClient::print_descriptor(struct gatt_db_attribute * attr, void * user_data)
{
  printf("\t\t  descr - handle: 0x%04x, uuid: ", gatt_db_attribute_get_handle(attr));
  BluetoothLEClient::print_uuid(gatt_db_attribute_get_type(attr));
}

void
BluetoothLEClient::print_characteristic(struct gatt_db_attribute * attr, void * user_data)
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
  BluetoothLEClient::print_uuid(&uuid);

  gatt_db_service_foreach_desc(attr, print_descriptor, nullptr);
}

void
BluetoothLEClient::print_service(struct gatt_db_attribute * attr, void * user_data)
{
  BluetoothLEClient *This = (BluetoothLEClient *) user_data;

  uint16_t start;
  uint16_t end;
  bool primary;
  bt_uuid_t uuid;

  if (!gatt_db_attribute_get_service_data(attr, &start, &end, &primary, &uuid)) {
    return;
  }

  printf("Service - start: 0x%04x, end: 0x%04x, type: %s, uuid: ", start, end, primary ? "primary" : "secondary");
  BluetoothLEClient::print_uuid(&uuid);

  gatt_db_service_foreach_incl(attr, print_included_data, This);
  gatt_db_service_foreach_char(attr, print_characteristic, nullptr);

  printf("\n");
}

void
BluetoothLEClient::ready_cb(bool success, uint8_t att_ecode, void * user_data)
{
  BluetoothLEClient * This = (BluetoothLEClient *) user_data;

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
BluetoothLEClient::service_changed_cb(uint16_t start_handle, uint16_t end_handle, void * user_data)
{
  BluetoothLEClient * This = (BluetoothLEClient *) user_data;

  printf("Service Changed handled - start: 0x%04x end: 0x%04x\n", start_handle, end_handle);
  gatt_db_foreach_service_in_range(This->db_, nullptr, print_service, This, start_handle, end_handle);
}

void
BluetoothLEClient::read_multiple_cb(
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
BluetoothLEClient::read_multiple(uint16_t * handles, uint8_t num_handles)
{
  if (!bt_gatt_client_is_ready(gatt_)) {
    throw std::runtime_error("GATT client not initialized");
  }

  if (!bt_gatt_client_read_multiple(gatt_, handles, num_handles, read_multiple_cb, nullptr, nullptr)) {
    printf("Failed to initiate read multiple procedure\n");
  }
}

void
BluetoothLEClient::read_cb(bool success, uint8_t att_ecode, const uint8_t * value, uint16_t length, void * /*user_data*/)
{
  if (!success) {
    printf("Read request failed: %s (0x%02x)\n", bluetooth::utils::to_string(att_ecode), att_ecode);
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
BluetoothLEClient::read_value(uint16_t handle)
{
  if (!bt_gatt_client_is_ready(gatt_)) {
    throw std::runtime_error("GATT client not initialized");
  }

  if (!bt_gatt_client_read_value(gatt_, handle, read_cb, nullptr, nullptr)) {
    printf("Failed to initiate read value\n");
  }
}

void
BluetoothLEClient::read_long_value(uint16_t handle, uint16_t offset)
{
  if (!bt_gatt_client_is_ready(gatt_)) {
    throw std::runtime_error("GATT client not initialized");
  }

  if (!bt_gatt_client_read_long_value(gatt_, handle, offset, read_cb, nullptr, nullptr)) {
    printf("Failed to initiate read long value\n");
  }
}

void
BluetoothLEClient::write_cb(bool success, uint8_t att_ecode, void * user_data)
{
  if (!success) {
    printf("Write failed: %s (0x%02x)\n", bluetooth::utils::to_string(att_ecode), att_ecode);
  }
}

void
BluetoothLEClient::write_long_cb(bool success, bool reliable_error, uint8_t att_ecode, void * /*user_data*/)
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
BluetoothLEClient::write_long_value(bool reliable_writes, uint16_t handle, uint16_t offset, uint8_t * value, int length)
{
  if (!bt_gatt_client_is_ready(gatt_)) {
    throw std::runtime_error("GATT client not initialized");
  }

  if (!bt_gatt_client_write_long_value(gatt_, reliable_writes, handle,
      offset, value, length, write_long_cb, nullptr, nullptr))
  {
    printf("Failed to initiate long write procedure\n");
  }
}

void
BluetoothLEClient::write_prepare(unsigned int id, uint16_t handle, uint16_t offset, uint8_t * value, unsigned int length)
{
  if (!bt_gatt_client_is_ready(gatt_)) {
    throw std::runtime_error("GATT client not initialized");
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
BluetoothLEClient::write_execute(unsigned int session_id, bool execute)
{
  if (!bt_gatt_client_is_ready(gatt_)) {
    throw std::runtime_error("GATT client not initialized");
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
BluetoothLEClient::notify_cb(
  uint16_t value_handle, const uint8_t * value,
  uint16_t length, void * user_data)
{
  printf("Handle Value Not/Ind: 0x%04x - ", value_handle);

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
BluetoothLEClient::register_notify_cb(uint16_t att_ecode, void * /*user_data*/)
{
  if (att_ecode) {
    printf("Failed to register notify handler - error code: 0x%02x\n", att_ecode);
    return;
  }

  printf("Registered notify handler!");
}

// TODO(mjeronimo): return unsigned int?
void
BluetoothLEClient::register_notify(uint16_t value_handle)
{
  if (!bt_gatt_client_is_ready(gatt_)) {
    throw std::runtime_error("GATT client not initialized");
  }

  unsigned int id = bt_gatt_client_register_notify(
    gatt_, value_handle, register_notify_cb, notify_cb, nullptr, nullptr);

  if (!id) {
    printf("Failed to register notify handler\n");
    return;
  }
}

void
BluetoothLEClient::unregister_notify(unsigned int id)
{
  if (!bt_gatt_client_is_ready(gatt_)) {
    throw std::runtime_error("GATT client not initialized");
  }

  if (!bt_gatt_client_unregister_notify(gatt_, id)) {
    printf("Failed to unregister notify handler with id: %u\n", id);
  }
}

void
BluetoothLEClient::set_security(int level)
{
  if (!bt_gatt_client_is_ready(gatt_)) {
    throw std::runtime_error("GATT client not initialized");
  }

  if (level < 1 || level > 3) {
    printf("Invalid level: %d\n", level);
    return;
  }

  if (!bt_gatt_client_set_security(gatt_, level)) {
    printf("Could not set security level\n");
  }
}

int
BluetoothLEClient::get_security()
{
  if (!bt_gatt_client_is_ready(gatt_)) {
    throw std::runtime_error("GATT client not initialized");
  }

  return bt_gatt_client_get_security(gatt_);
}

bool
BluetoothLEClient::local_counter(uint32_t * sign_cnt, void * /*user_data*/)
{
  static uint32_t cnt = 0;
  *sign_cnt = cnt++;
  return true;
}

void
BluetoothLEClient::set_sign_key(uint8_t key[16])
{
  bt_att_set_local_key(att_, key, local_counter, this);
}

/**
 * create a bluetooth le l2cap socket and connect src to dst
 *
 * @param src  6 bytes BD Address source address
 * @param dst  6 bytes BD Address destination address
 * @param dst_type  destination type BDADDR_LE_PUBLIC or BDADDR_LE_RANDOM
 * @param sec  security level BT_SECURITY_LOW or BT_SECURITY_MEDIUM or BT_SECURITY_HIGH
 * @return socket or -1 if error
 */
int
BluetoothLEClient::l2cap_le_att_connect(bdaddr_t * src, bdaddr_t * dst, uint8_t dst_type, int sec)
{
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
    perror("BluetoothLEClient: Failed to create L2CAP socket");
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
    perror("BluetoothLEClient: Failed to bind L2CAP socket");
    close(sock);
    return -1;
  }

  // Set the security level
  struct sockaddr_l2 dstaddr;

  struct bt_security btsec;
  memset(&btsec, 0, sizeof(btsec));
  btsec.level = sec;

  if (setsockopt(sock, SOL_BLUETOOTH, BT_SECURITY, &btsec, sizeof(btsec)) != 0) {
    fprintf(stderr, "BluetoothLEClient: Failed to set L2CAP security level\n");
    close(sock);
    return -1;
  }

  // Set up destination address
  memset(&dstaddr, 0, sizeof(dstaddr));
  dstaddr.l2_family = AF_BLUETOOTH;
  dstaddr.l2_cid = htobs(ATT_CID);
  dstaddr.l2_bdaddr_type = dst_type;
  bacpy(&dstaddr.l2_bdaddr, dst);

  printf("BluetoothLEClient: Connecting to device (%s)...\n", dstaddr_str);
  fflush(stdout);

  if (connect(sock, (struct sockaddr *) &dstaddr, sizeof(dstaddr)) < 0) {
    printf("BluetoothLEClient: Failed to connect to device\n");
    close(sock);
    return -1;
  }

  printf("BluetoothLEClient: Connected\n");
  return sock;
}

void
BluetoothLEClient::write_value(uint16_t handle, uint8_t * value, int length, bool without_response, bool signed_write)
{
  if (!bt_gatt_client_is_ready(gatt_)) {
    throw std::runtime_error("GATT client not initialized");
  }

  if (without_response) {
    if (!bt_gatt_client_write_without_response(gatt_, handle, signed_write, value, length)) {
      printf("Failed to initiate write-without-response procedure\n");
    }
  } else {
    if (!bt_gatt_client_write_value(gatt_, handle, value, length, write_cb, nullptr, nullptr)) {
      printf("Failed to initiate write procedure\n");
    }
  }
}

void
BluetoothLEClient::process_input()
{
  mainloop_run();
}

}  // namespace bluetooth