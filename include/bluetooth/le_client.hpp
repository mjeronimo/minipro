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

#ifndef BLUETOOTH__LE_CLIENT_HPP_
#define BLUETOOTH__LE_CLIENT_HPP_

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
#include "gatt-client.h"
}

#include "bluetooth/l2_cap_socket.hpp"

namespace bluetooth {

class LEClient
{
public:
  LEClient(const std::string & device_address, uint8_t dst_type = BDADDR_LE_RANDOM, int sec = BT_SECURITY_LOW, uint16_t mtu = 0);

  // GattClient
  static void ready_cb(bool success, uint8_t att_ecode, void * user_data);
  static void service_added_cb(struct gatt_db_attribute * attr, void * user_data);
  static void service_changed_cb(uint16_t start_handle, uint16_t end_handle, void * user_data);
  static void service_removed_cb(struct gatt_db_attribute * attr, void * user_data);
  static void att_disconnect_cb(int err, void * user_data);

  ~LEClient();

  int get_security();
  void set_security(int level);	// BT_SECURITY_SDP, LOW, MEDIUM, HIGH

  void read_long_value(uint16_t handle, uint16_t value);

  void read_multiple(uint16_t * handles, uint8_t num_handles);
  static void read_multiple_cb(bool success, uint8_t att_ecode, const uint8_t * value, uint16_t length, void * user_data);

  void read_value(uint16_t handle);
  static void read_cb(bool success, uint8_t att_ecode, const uint8_t * value, uint16_t length, void * user_data);

  void register_notify(uint16_t value_handle);
  static void notify_cb(uint16_t value_handle, const uint8_t * value, uint16_t length, void * user_data);
  static void register_notify_cb(uint16_t att_ecode, void * user_data);

  void set_sign_key(uint8_t key[16]);
  static bool local_counter(uint32_t * sign_cnt, void * user_data);

  void unregister_notify(unsigned int id);

  void write_execute(unsigned int session_id, bool execute);

  void write_long_value(bool reliable_writes, uint16_t handle, uint16_t offset, uint8_t * value, int length);
  static void write_long_cb(bool success, bool reliable_error, uint8_t att_ecode, void * user_data);

  void write_prepare(unsigned int id, uint16_t handle, uint16_t offset, uint8_t * value, unsigned int length);
  void write_value(uint16_t handle, uint8_t * value, int length, bool without_response = false, bool signed_write = false);
  static void write_cb(bool success, uint8_t att_ecode, void * user_data);

protected:
  // Bluetooth socket
  int fd_{-1};                       
  struct bt_att * att_{nullptr};
  std::unique_ptr<L2CapSocket> l2_cap_socket_;

  // GattClient
  struct gatt_db * db_{nullptr};
  struct bt_gatt_client * gatt_{nullptr};
  unsigned int reliable_session_id_{0};
  void process_input();
  std::unique_ptr<std::thread> input_thread_;

  std::mutex mutex_;
  std::condition_variable cv_;
  bool ready_{false};

public:
  // TODO(mjeronimo): move to utils (or GattClient)
  static void print_uuid(const bt_uuid_t * uuid);
  static void print_included_data(struct gatt_db_attribute * attr, void * user_data);
  static void print_descriptor(struct gatt_db_attribute * attr, void * user_data);
  static void print_characteristic(struct gatt_db_attribute * attr, void * user_data);
  static void print_service(struct gatt_db_attribute * attr, void * user_data);
};

}  // namespace bluetooth

#endif  // BLUETOOTH__LE_CLIENT_HPP_

