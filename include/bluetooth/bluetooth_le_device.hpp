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

#include <memory>
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
  void quit(char * cmd_str);
  void read_long_value(char * cmd_str);
  void read_multiple(char * cmd_str);
  void read_value(char * cmd_str);
  void register_notify(uint16_t value_handle);
  void set_security(char * cmd_str);
  void set_sign_key(char * cmd_str);
  void unregister_notify(unsigned int id);
  void write_execute(char * cmd_str);
  void write_long_value(char * cmd_str);
  void write_prepare(char * cmd_str);
  void write_value(char * cmd_str);

private:
  // TODO: inline the client structure here
  struct client * cli;
  int fd;

  void process_input();
  std::unique_ptr<std::thread> input_thread_;

public:
  static void att_disconnect_cb(int err, void * user_data);
  static void att_debug_cb(const char * str, void * user_data);
};

}

#endif  // BLUETOOTH_BLUETOOTH_LE_DEVICE_HPP_

