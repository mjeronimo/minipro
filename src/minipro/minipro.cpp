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

#include "minipro/minipro.hpp"
#include "minipro/packet.hpp"

#include "minipro/drive.hpp"
#include "minipro/enter_remote_control_mode.hpp"
#include "minipro/exit_remote_control_mode.hpp"

#include <string>
#include <vector>

namespace jeronibot
{
namespace minipro
{

MiniPro::MiniPro(const std::string & bt_addr)
: BluetoothLEClient(bt_addr)
{
}

units::velocity::miles_per_hour_t
MiniPro::get_current_speed()
{
  // TODO(mjeronimo):
  return 0_mph;
}

units::current::ampere_t
MiniPro::get_battery_level()
{
  // TODO(mjeronimo):
  return 0_A;
}

units::voltage::volt_t
MiniPro::get_voltage()
{
  // TODO(mjeronimo):
  return 0_V;
}

units::temperature::fahrenheit_t
MiniPro::get_vehicle_temperature()
{
  // TODO(mjeronimo):
  return 0_degF;
}

void
MiniPro::enable_notifications()
{
  uint16_t service_handle = 0x000c;

  // TODO: htons
  uint8_t cmd_buf[2]{0x01, 0x00};

  write_value(service_handle, cmd_buf, sizeof(cmd_buf));
}

void
MiniPro::disable_notifications()
{
  // TODO(mjeronimo):
}

void
MiniPro::enter_remote_control_mode()
{
  packet::EnterRemoteControlMode packet;
  std::vector<uint8_t> bytes = packet.get_bytes();

  uint16_t service_handle = 0x000e;
  bool without_response = true;

  write_value(service_handle, bytes.data(), bytes.size(), without_response);
}

void
MiniPro::exit_remote_control_mode()
{
  packet::ExitRemoteControlMode packet;
  std::vector<uint8_t> bytes = packet.get_bytes();

  uint16_t service_handle = 0x000e;
  bool without_response = true;

  write_value(service_handle, bytes.data(), bytes.size(), without_response);
}

void
MiniPro::drive(int16_t throttle, int16_t steering)
{
  packet::Drive packet;
  // packet.setThrottle(throttle);
  // packet.setSteering(steering);
  
  std::vector<uint8_t> bytes = packet.get_bytes();

  uint16_t sum = 0x06 + 0xa + 0x03 + 0x7b;

  unsigned char * p_a0 = (unsigned char *) &throttle;
  sum += p_a0[0] + p_a0[1];

  unsigned char * p_a1 = (unsigned char *) &steering;
  sum += p_a1[0] + p_a1[1];

  uint16_t checksum = sum ^ 0xffff;
  unsigned char * p_checksum = (unsigned char *) &checksum;

  // TODO(mjeronimo): htons, etc.
  uint8_t cmd_buf[12]{0x55, 0xaa, 0x06, 0x0a, 0x03, 0x7b, p_a0[0], p_a0[1], p_a1[0], p_a1[1], p_checksum[0], p_checksum[1]};

  uint16_t service_handle = 0x000e;
  bool without_response = true;

  write_value(service_handle, cmd_buf, sizeof(cmd_buf), without_response);
}

}  // namespace minipro
}  // namespace jeronibot
