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
#include "util/ansi_colors.hpp"
#include "util/joystick.hpp"
#include "bluetooth/utils.hpp"

using namespace std::chrono_literals;

namespace bluetooth
{

BluetoothLEDevice::BluetoothLEDevice(
  const std::string & device_address, uint8_t dst_type, int sec,
  uint16_t mtu)
{
  bool verbose = false;
  bdaddr_t bdaddr_any = {{0, 0, 0, 0, 0, 0}};

  bdaddr_t dst_addr;
  str2ba(device_address.c_str(), &dst_addr);

  bdaddr_t src_addr;
  bacpy(&src_addr, &bdaddr_any);

  mainloop_init();

  fd = l2cap_le_att_connect(&src_addr, &dst_addr, dst_type, sec, verbose);
  if (fd < 0) {
    throw std::runtime_error("BluetoothLEDevice: Failed to connect to Bluetooth device");
  }

  cli = client_create(fd, mtu, verbose);
  if (!cli) {
    close(fd);
    throw std::runtime_error("BluetoothLEDevice: client_create failed");
  }

  bt_gatt_client_set_ready_handler(cli->gatt, ready_cb, this, nullptr);
  bt_gatt_client_set_service_changed(cli->gatt, service_changed_cb, this, nullptr);

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
  client_destroy(cli);
  printf("BluetoothLEDevice::~BluetoothLEDevice: after client_destroy\n");
}

void
BluetoothLEDevice::att_disconnect_cb(int err, void * /*user_data*/)
{
  printf("Device disconnected: %s\n", strerror(err));
  mainloop_quit();
}

// Print a prefix (user_data) followed by a message (str)
// Example att: ATT op 0x02 (att: is the prefix)
void
BluetoothLEDevice::att_debug_cb(const char * str, void * user_data)
{
  const char * prefix = (const char *) user_data;
  printf(COLOR_BOLDGRAY "%s" COLOR_BOLDWHITE "%s\n" COLOR_OFF, prefix, str);
}

// Print a prefix (user_data) followed by a message (str)
// Example gatt: MTU exchange complete, with MTU: 23 (gatt: is the prefix)
void
BluetoothLEDevice::gatt_debug_cb(const char * str, void * user_data)
{
  const char * prefix = (const char *) user_data;
  printf(COLOR_GREEN "%s%s\n" COLOR_OFF, prefix, str);
}

void
BluetoothLEDevice::service_added_cb(struct gatt_db_attribute * attr, void * user_data)
{
}

void
BluetoothLEDevice::service_removed_cb(struct gatt_db_attribute * attr, void * user_data)
{
}

// Class GattClient
/**
 * create an gatt client attached to the fd l2cap socket
 *
 * @param fd  socket
 * @param mtu  selected pdu size
 * @return gatt client structure
 */
struct client *
BluetoothLEDevice::client_create(int fd, uint16_t mtu, bool verbose)
{
  struct client * cli = new0(struct client, 1);

  if (!cli) {
    fprintf(stderr, "Failed to allocate memory for client\n");
    return nullptr;
  }

  cli->att = bt_att_new(fd, false);
  if (!cli->att) {
    fprintf(stderr, "Failed to initialze ATT transport layer\n");
    bt_att_unref(cli->att);
    free(cli);
    return nullptr;
  }

  if (!bt_att_set_close_on_unref(cli->att, true)) {
    fprintf(stderr, "Failed to set up ATT transport layer\n");
    bt_att_unref(cli->att);
    free(cli);
    return nullptr;
  }

  if (!bt_att_register_disconnect(
      cli->att, BluetoothLEDevice::att_disconnect_cb, nullptr,
      nullptr))
  {
    fprintf(stderr, "Failed to set ATT disconnect handler\n");
    bt_att_unref(cli->att);
    free(cli);
    return nullptr;
  }

  cli->fd = fd;
  cli->db = gatt_db_new();
  if (!cli->db) {
    fprintf(stderr, "Failed to create GATT database\n");
    bt_att_unref(cli->att);
    free(cli);
    return nullptr;
  }

  cli->gatt = bt_gatt_client_new(cli->db, cli->att, mtu);
  if (!cli->gatt) {
    fprintf(stderr, "Failed to create GATT client\n");
    gatt_db_unref(cli->db);
    bt_att_unref(cli->att);
    free(cli);
    return nullptr;
  }

  gatt_db_register(cli->db, service_added_cb, service_removed_cb, nullptr, nullptr);

  if (verbose) {
    bt_att_set_debug(
      cli->att, BluetoothLEDevice::att_debug_cb,
      reinterpret_cast<void *>(const_cast<char *>("att: ")), nullptr);
    bt_gatt_client_set_debug(
      cli->gatt, gatt_debug_cb,
      reinterpret_cast<void *>(const_cast<char *>("gatt: ")), nullptr);
  }

  // bt_gatt_client_set_ready_handler(cli->gatt, ready_cb, cli, nullptr);
  // bt_gatt_client_set_service_changed(cli->gatt, service_changed_cb, cli, nullptr);

  // bt_gatt_client already holds a reference
  gatt_db_unref(cli->db);

  return cli;
}

void
BluetoothLEDevice::client_destroy(struct client * cli)
{
  bt_gatt_client_unref(cli->gatt);
  bt_att_unref(cli->att);
  free(cli);
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
  struct client * cli = (struct client *) user_data;
  uint16_t handle, start, end;
  struct gatt_db_attribute * service;
  bt_uuid_t uuid;

  if (!gatt_db_attribute_get_incl_data(attr, &handle, &start, &end)) {
    return;
  }

  service = gatt_db_get_attribute(cli->db, start);
  if (!service) {
    return;
  }

  gatt_db_attribute_get_service_uuid(service, &uuid);

  printf(
    "\t  " COLOR_GREEN "include" COLOR_OFF " - handle: "
    "0x%04x, - start: 0x%04x, end: 0x%04x,"
    "uuid: ", handle, start, end);
  BluetoothLEDevice::print_uuid(&uuid);
}

void
BluetoothLEDevice::print_descriptor(struct gatt_db_attribute * attr, void * user_data)
{
  printf(
    "\t\t  " COLOR_MAGENTA "descr" COLOR_OFF " - handle: 0x%04x, uuid: ",
    gatt_db_attribute_get_handle(attr));
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

  printf(
    "\t  " COLOR_YELLOW "charac" COLOR_OFF
    " - start: 0x%04x, value: 0x%04x, "
    "props: 0x%02x, uuid: ",
    handle, value_handle, properties);
  BluetoothLEDevice::print_uuid(&uuid);

  gatt_db_service_foreach_desc(attr, print_descriptor, nullptr);
}

void
BluetoothLEDevice::print_service(struct gatt_db_attribute * attr, void * user_data)
{
  struct client * cli = (struct client *) user_data;
  uint16_t start, end;
  bool primary;
  bt_uuid_t uuid;

  if (!gatt_db_attribute_get_service_data(attr, &start, &end, &primary, &uuid)) {
    return;
  }

  printf(
    COLOR_RED "service" COLOR_OFF " - start: 0x%04x, "
    "end: 0x%04x, type: %s, uuid: ",
    start, end, primary ? "primary" : "secondary");
  BluetoothLEDevice::print_uuid(&uuid);

  gatt_db_service_foreach_incl(attr, print_included_data, cli);
  gatt_db_service_foreach_char(attr, print_characteristic, nullptr);

  printf("\n");
}

void
BluetoothLEDevice::ready_cb(bool success, uint8_t att_ecode, void * user_data)
{
  // struct client * cli = (struct client *) user_data;
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
  struct client * cli = (struct client *) user_data;
  printf("\nService Changed handled - start: 0x%04x end: 0x%04x\n", start_handle, end_handle);
  gatt_db_foreach_service_in_range(cli->db, nullptr, print_service, cli, start_handle, end_handle);
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

/**
 * read multiple callback
 *
 * @param success    ==0 => print error, <>0 print values
 * @param att_ecode   att error code
 * @param value      vector of values
 * @param length    number of values
 * @param user_data    not used
 */
void
BluetoothLEDevice::read_multiple_cb(
  bool success, uint8_t att_ecode, const uint8_t * value, uint16_t length,
  void * user_data)
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
BluetoothLEDevice::read_multiple(char * cmd_str)
{
  int argc = 0;
  char * argv[512];
  char * endptr = nullptr;

  if (!bt_gatt_client_is_ready(cli->gatt)) {
    printf("GATT client not initialized\n");
    return;
  }

  if (!parse_args(cmd_str, sizeof(argv), argv, &argc) || argc < 2) {
    return;
  }

  uint16_t * value = reinterpret_cast<uint16_t *>(malloc(sizeof(uint16_t) * argc));
  if (!value) {
    printf("Failed to construct value\n");
    return;
  }

  for (int i = 0; i < argc; i++) {
    value[i] = strtol(argv[i], &endptr, 0);
    if (endptr == argv[i] || *endptr != '\0' || !value[i]) {
      printf("Invalid value byte: %s\n", argv[i]);
      free(value);
      return;
    }
  }

  if (!bt_gatt_client_read_multiple(cli->gatt, value, argc, read_multiple_cb, nullptr, nullptr)) {
    printf("Failed to initiate read multiple procedure\n");
  }

  free(value);
}

/**
 * read value callback
 *
 * @param success    ==0 => print error, <>0 print value
 * @param att_ecode    att error code
 * @param value      vector of values
 * @param length    size of vector
 * @param user_data    not used
 */
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
  if (!bt_gatt_client_is_ready(cli->gatt)) {
    printf("GATT client not initialized\n");
    return;
  }

  if (!bt_gatt_client_read_value(cli->gatt, handle, read_cb, nullptr, nullptr)) {
    printf("Failed to initiate read value\n");
  }
}

void
BluetoothLEDevice::read_long_value(uint16_t handle, uint16_t offset)
{
  if (!bt_gatt_client_is_ready(cli->gatt)) {
    printf("GATT client not initialized\n");
    return;
  }

  if (!bt_gatt_client_read_long_value(cli->gatt, handle, offset, read_cb, nullptr, nullptr)) {
    printf("Failed to initiate read long value\n");
  }
}

static struct option write_value_options[] = {
  {"without-response", 0, 0, 'w'},
  {"signed-write", 0, 0, 's'},
  {}
};

/**
 * write value call back
 *
 * @param success    ==0 => print error, <>0 print value
 * @param att_ecode    att error code
 * @param user_data    not used
 */

void
BluetoothLEDevice::write_cb(bool success, uint8_t att_ecode, void * user_data)
{
  if (!success) {
    printf(
      "\nWrite failed: %s (0x%02x)\n", bluetooth::utils::to_string(att_ecode),
      att_ecode);
  }
}


static struct option write_long_value_options[] = {
  {"reliable-write", 0, 0, 'r'},
  {}
};

/**
 * write long call back
 *
 * @param success      ==0 => print error, <>0 print value
 * @param reliable_error  status of the write operation when failed
 * @param att_ecode      att error code
 * @param user_data      not used
 */
void
BluetoothLEDevice::write_long_cb(bool success, bool reliable_error, uint8_t att_ecode, void * user_data)
{
  if (success) {
    // printf("Write successful\n");
  } else if (reliable_error) {
    printf("Reliable write not verified\n");
  } else {
    printf(
      "\nWrite failed: %s (0x%02x)\n", bluetooth::utils::to_string(att_ecode),
      att_ecode);
  }
}

/**
 * write long value command
 *
 * @param cli    pointer to the client structure
 * @param cmd_str  command string for write long value
 */
void
BluetoothLEDevice::write_long_value(char * cmd_str)
{
  int opt, i;
  char * argvbuf[516];
  char ** argv = argvbuf;
  int argc = 1;
  char * endptr = nullptr;
  uint8_t * value = nullptr;
  bool reliable_writes = false;

  if (!bt_gatt_client_is_ready(cli->gatt)) {
    printf("GATT client not initialized\n");
    return;
  }

  if (!parse_args(cmd_str, 514, argv + 1, &argc)) {
    printf("Too many arguments\n");
    return;
  }

  optind = 0;
  argv[0] = (char *) "write-long-value";
  while ((opt = getopt_long(
      argc, argv, "+r", write_long_value_options,
      nullptr)) != -1)
  {
    switch (opt) {
      case 'r':
        reliable_writes = true;
        break;
      default:
        return;
    }
  }

  argc -= optind;
  argv += optind;

  if (argc < 2) {
    return;
  }

  uint16_t handle = strtol(argv[0], &endptr, 0);
  if (!endptr || *endptr != '\0' || !handle) {
    printf("Invalid handle: %s\n", argv[0]);
    return;
  }

  endptr = nullptr;
  uint16_t offset = strtol(argv[1], &endptr, 0);
  if (!endptr || *endptr != '\0' || errno == ERANGE) {
    printf("Invalid offset: %s\n", argv[1]);
    return;
  }

  int length = argc - 2;
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

    for (i = 2; i < argc; i++) {
      if (strlen(argv[i]) != 2) {
        printf(
          "Invalid value byte: %s\n",
          argv[i]);
        free(value);
        return;
      }

      value[i - 2] = strtol(argv[i], &endptr, 0);
      if (endptr == argv[i] || *endptr != '\0' ||
        errno == ERANGE)
      {
        printf(
          "Invalid value byte: %s\n",
          argv[i]);
        free(value);
        return;
      }
    }
  }

  if (!bt_gatt_client_write_long_value(
      cli->gatt, reliable_writes, handle,
      offset, value, length,
      write_long_cb,
      nullptr, nullptr))
  {
    printf("Failed to initiate long write procedure\n");
  }

  free(value);
}

static struct option write_prepare_options[] = {
  {"session-id", 1, 0, 's'},
  {}
};

/**
 * write prepare command
 *
 * @param cli    pointer to the client structure
 * @param cmd_str  write prepare command string
 */
void
BluetoothLEDevice::write_prepare(char * cmd_str)
{
  int opt, i;
  char * argvbuf[516];
  char ** argv = argvbuf;
  int argc = 0;
  unsigned int id = 0;
  char * endptr = nullptr;
  uint8_t * value = nullptr;

  if (!bt_gatt_client_is_ready(cli->gatt)) {
    printf("GATT client not initialized\n");
    return;
  }

  if (!parse_args(cmd_str, 514, argv + 1, &argc)) {
    printf("Too many arguments\n");
    return;
  }

  /* Add command name for getopt_long */
  argc++;
  argv[0] = (char *) "write-prepare";

  optind = 0;
  while ((opt = getopt_long(
      argc, argv, "s:", write_prepare_options,
      nullptr)) != -1)
  {
    switch (opt) {
      case 's':
        if (!optarg) {
          return;
        }

        id = atoi(optarg);

        break;
      default:
        return;
    }
  }

  argc -= optind;
  argv += optind;

  if (argc < 3) {
    return;
  }

  if (cli->reliable_session_id != id) {
    printf(
      "Session id != Ongoing session id (%u!=%u)\n", id,
      cli->reliable_session_id);
    return;
  }

  uint16_t handle = strtol(argv[0], &endptr, 0);
  if (!endptr || *endptr != '\0' || !handle) {
    printf("Invalid handle: %s\n", argv[0]);
    return;
  }

  endptr = nullptr;
  uint16_t offset = strtol(argv[1], &endptr, 0);
  if (!endptr || *endptr != '\0' || errno == ERANGE) {
    printf("Invalid offset: %s\n", argv[1]);
    return;
  }

  /*
   * First two arguments are handle and offset. What remains is the value
   * length
   */
  unsigned int length = argc - 2;

  if (length == 0) {
    goto done;
  }

  if (length > UINT16_MAX) {
    printf("Write value too long\n");
    return;
  }

  value = reinterpret_cast<uint8_t *>(malloc(length));
  if (!value) {
    printf("Failed to allocate memory for value\n");
    return;
  }

  for (i = 2; i < argc; i++) {
    if (strlen(argv[i]) != 2) {
      printf("Invalid value byte: %s\n", argv[i]);
      free(value);
      return;
    }

    value[i - 2] = strtol(argv[i], &endptr, 0);
    if (endptr == argv[i] || *endptr != '\0' || errno == ERANGE) {
      printf("Invalid value byte: %s\n", argv[i]);
      free(value);
      return;
    }
  }

done:
  cli->reliable_session_id =
    bt_gatt_client_prepare_write(
    cli->gatt, id,
    handle, offset,
    value, length,
    write_long_cb, nullptr,
    nullptr);

  if (!cli->reliable_session_id) {
    printf("Failed to proceed prepare write\n");
  } else {
    printf(
      "Prepare write success.\n"
      "Session id: %d to be used on next write\n",
      cli->reliable_session_id);
  }

  free(value);
}

/**
 * write execute command
 *
 * @param cli    pointer to the client structure
 * @param cmd_str  write execute command string
 */
void
BluetoothLEDevice::write_execute(char * cmd_str)
{
  char * argvbuf[516];
  char ** argv = argvbuf;
  int argc = 0;
  char * endptr = nullptr;

  if (!bt_gatt_client_is_ready(cli->gatt)) {
    printf("GATT client not initialized\n");
    return;
  }

  if (!parse_args(cmd_str, 514, argv, &argc)) {
    printf("Too many arguments\n");
    return;
  }

  if (argc < 2) {
    return;
  }

  unsigned int session_id = strtol(argv[0], &endptr, 0);
  if (!endptr || *endptr != '\0') {
    printf("Invalid session id: %s\n", argv[0]);
    return;
  }

  if (session_id != cli->reliable_session_id) {
    printf(
      "Invalid session id: %u != %u\n", session_id,
      cli->reliable_session_id);
    return;
  }

  bool execute = !!strtol(argv[1], &endptr, 0);
  if (!endptr || *endptr != '\0') {
    printf("Invalid execute: %s\n", argv[1]);
    return;
  }

  if (execute) {
    if (!bt_gatt_client_write_execute(cli->gatt, session_id, write_cb, nullptr, nullptr)) {
      printf("Failed to proceed write execute\n");
    }
  } else {
    bt_gatt_client_cancel(cli->gatt, session_id);
  }

  cli->reliable_session_id = 0;
}

/**
 * notify call back
 *
 * @param value_handle  handle of the notifying object
 * @param value      vector value of the object
 * @param length    length of vector value
 * @param user_data    not used
 */
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
  if (!bt_gatt_client_is_ready(cli->gatt)) {
    printf("GATT client not initialized\n");
    return;
  }

  unsigned int id = bt_gatt_client_register_notify(
    cli->gatt, value_handle,
    register_notify_cb, notify_cb, nullptr, nullptr);
  if (!id) {
    printf("Failed to register notify handler\n");
    return;
  }

  printf("Registering notify handler with id: %u\n", id);
}

void
BluetoothLEDevice::unregister_notify(unsigned int id)
{
  if (!bt_gatt_client_is_ready(cli->gatt)) {
    printf("GATT client not initialized\n");
    return;
  }

  if (!bt_gatt_client_unregister_notify(cli->gatt, id)) {
    printf("Failed to unregister notify handler with id: %u\n", id);
  } else {
    printf("Unregistered notify handler with id: %u\n", id);
  }
}

void
BluetoothLEDevice::set_security(int level)
{
  if (!bt_gatt_client_is_ready(cli->gatt)) {
    printf("GATT client not initialized\n");
    return;
  }

  if (level < 1 || level > 3) {
    printf("Invalid level: %d\n", level);
    return;
  }

  if (!bt_gatt_client_set_security(cli->gatt, level)) {
    printf("Could not set security level\n");
  } else {
    printf("Setting security level %d success\n", level);
  }
}

void
BluetoothLEDevice::get_security()
{
  if (!bt_gatt_client_is_ready(cli->gatt)) {
    printf("GATT client not initialized\n");
    return;
  }

  int level = bt_gatt_client_get_security(cli->gatt);

  if (level < 0) {
    printf("Could not get security level\n");
  } else {
    printf("Security level: %u\n", level);
  }
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
  bt_att_set_local_key(cli->att, key, local_counter, cli);
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
BluetoothLEDevice::l2cap_le_att_connect(bdaddr_t * src, bdaddr_t * dst, uint8_t dst_type, int sec, bool verbose)
{
  struct bt_security btsec;

  char dstaddr_str[18];
  ba2str(dst, dstaddr_str);

  if (verbose) {
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

  if (!bt_gatt_client_is_ready(cli->gatt)) {
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
    if (!bt_gatt_client_write_without_response(cli->gatt, handle, signed_write, value, length)) {
      printf("Failed to initiate write without response procedure\n");
      goto done;
    }
    goto done;
  }

  if (!bt_gatt_client_write_value(cli->gatt, handle, value, length, write_cb, nullptr, nullptr)) {
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
