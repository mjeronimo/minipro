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

#include <netinet/in.h>

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
  const uint16_t service_handle = 0x000c;
  uint16_t enable = htons(0x0001);
  write_value(service_handle, (uint8_t *) &enable, sizeof(enable));
}

void
MiniPro::disable_notifications()
{
  const uint16_t service_handle = 0x000c;
  uint16_t enable = htons(0x0000);
  write_value(service_handle, (uint8_t *) &enable, sizeof(enable));
}

void
MiniPro::enter_remote_control_mode()
{
  packet::EnterRemoteControlMode packet;
  send_packet(packet);
}

void
MiniPro::exit_remote_control_mode()
{
  packet::ExitRemoteControlMode packet;
  send_packet(packet);
}

void
MiniPro::drive(int16_t throttle, int16_t steering)
{
  packet::Drive packet;
  packet.setThrottle(throttle);
  packet.setSteering(steering);
  send_packet(packet);
}

void
MiniPro::send_packet(packet::Packet & packet)
{
  std::vector<uint8_t> bytes = packet.get_bytes();
  write_value(tx_service_handle_, bytes.data(), bytes.size(), true);
}

}  // namespace minipro
}  // namespace jeronibot
