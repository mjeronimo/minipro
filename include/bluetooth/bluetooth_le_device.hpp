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

#ifndef BLUETOOTH_BLUETOOTH_LE_DEVICE_HPP_
#define BLUETOOTH_BLUETOOTH_LE_DEVICE_HPP_

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <stdint.h>

extern "C" {
#include "att.h"
#include "bluetooth.h"
#include "uuid.h"
#include "gatt-db.h"
}

// Client structure to hold gatt client context
struct client {
  int fd;                           // socket
  struct bt_att *att;               // pointer to a bt_att structure
  struct gatt_db *db;               // pointer to a gatt_db structure
  struct bt_gatt_client *gatt;      // pointer to a bt_gatt_client structure
  unsigned int reliable_session_id; // session id
};

namespace bluetooth {

class BluetoothLEDevice
{
public:
  BluetoothLEDevice(const std::string & device_address, uint8_t dst_type = BDADDR_LE_RANDOM, int sec = BT_SECURITY_LOW, uint16_t mtu = 0);
  ~BluetoothLEDevice();

  void get_security();
  void read_long_value(uint16_t handle, uint16_t value);
  void read_multiple(char * cmd_str);
  void read_value(uint16_t handle);
  void register_notify(uint16_t value_handle);
  void set_security(int level);
  void set_sign_key(uint8_t key[16]);
  void unregister_notify(unsigned int id);
  void write_execute(char * cmd_str);
  void write_long_value(char * cmd_str);
  void write_prepare(char * cmd_str);
  void write_value(char * cmd_str);

protected:
  // TODO: inline the client structure here
  struct client * cli{nullptr};
  int fd{-1};

  void process_input();
  std::unique_ptr<std::thread> input_thread_;

  std::mutex mutex_;
  std::condition_variable cv_;
  bool ready_{false};

public:
  static bool local_counter(uint32_t * sign_cnt, void * user_data);
  static int l2cap_le_att_connect(bdaddr_t * src, bdaddr_t * dst, uint8_t dst_type, int sec, bool verbose);
  static void ready_cb(bool success, uint8_t att_ecode, void * user_data);
  static void service_added_cb(struct gatt_db_attribute * attr, void * user_data);
  static void service_changed_cb(uint16_t start_handle, uint16_t end_handle, void * user_data);
  static void service_removed_cb(struct gatt_db_attribute * attr, void * user_data);

  static struct client * client_create(int fd, uint16_t mtu, bool verbose);
  static void client_destroy(struct client * cli);

  static bool parse_args(char * str, int expected_argc, char ** argv, int * argc);

  static void att_debug_cb(const char * str, void * user_data);
  static void att_disconnect_cb(int err, void * user_data);
  static void gatt_debug_cb(const char * str, void * user_data);
  static void notify_cb(uint16_t value_handle, const uint8_t * value, uint16_t length, void * user_data);
  static void read_multiple_cb(bool success, uint8_t att_ecode, const uint8_t * value, uint16_t length, void * user_data);
  static void read_cb(bool success, uint8_t att_ecode, const uint8_t * value, uint16_t length, void * user_data);
  static void register_notify_cb(uint16_t att_ecode, void * user_data);
  static void write_cb(bool success, uint8_t att_ecode, void * user_data);
  static void write_long_cb(bool success, bool reliable_error, uint8_t att_ecode, void * user_data);

  // TODO(mjeronimo): move to utils
  static void print_uuid(const bt_uuid_t * uuid);
  static void print_included_data(struct gatt_db_attribute * attr, void * user_data);
  static void print_descriptor(struct gatt_db_attribute * attr, void * user_data);
  static void print_characteristic(struct gatt_db_attribute * attr, void * user_data);
  static void print_service(struct gatt_db_attribute * attr, void * user_data);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_BLUETOOTH_LE_DEVICE_HPP_

