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

#include <string>

namespace jaymo
{
namespace minipro
{

MiniPro::MiniPro(const std::string & minipro_addr)
: BluetoothLEDevice(minipro_addr)
{
}

int
MiniPro::get_current_speed()
{
  // TODO(mjeronimo):
  return 0;
}

int
MiniPro::get_battery_level()
{
  // TODO(mjeronimo):
  return 0;
}

int
MiniPro::get_voltage()
{
  // TODO(mjeronimo):
  return 0;
}

int
MiniPro::get_vehicle_temperature()
{
  // TODO(mjeronimo):
  return 0;
}

void
MiniPro::enable_notifications()
{
  char enable_notifications_cmd[] = "0x000c 01 00";
  write_value(enable_notifications_cmd);
}

void
MiniPro::disable_notifications()
{
  // TODO(mjeronimo):
  // char enable_notifications_cmd[] = "0x000c 01 00";
  // write_value(enable_notifications_cmd);
}

void
MiniPro::enter_remote_control_mode()
{
  char enter_remote_control_cmd[] = "-w 0x000e 55 aa 04 0a 03 7a 01 00 73 ff";
  write_value(enter_remote_control_cmd);
}

void
MiniPro::exit_remote_control_mode()
{
  // TODO(mjeronimo):
  // char enter_remote_control_cmd[] = "-w 0x000e 55 aa 04 0a 03 7a 01 00 73 ff";
  // write_value(enter_remote_control_cmd);
}

void
MiniPro::drive(int16_t speed, int16_t angle)
{
  uint16_t sum = 0x06 + 0xa + 0x03 + 0x7b;

  unsigned char * p_a0 = (unsigned char *) &speed;
  sum += p_a0[0] + p_a0[1];

  unsigned char * p_a1 = (unsigned char *) &angle;
  sum += p_a1[0] + p_a1[1];

  uint16_t checksum = sum ^ 0xffff;
  unsigned char * p_checksum = (unsigned char *) &checksum;

  // TODO(mjeronimo): htons, etc.

  char cmd_buf[256];
  snprintf(
    cmd_buf, sizeof(cmd_buf), "-w 0x000e 55 aa 06 0a 03 7b %02X %02X %02X %02x %02X %02X",
    p_a0[0], p_a0[1],
    p_a1[0], p_a1[1],
    p_checksum[0], p_checksum[1]
  );

  write_value(cmd_buf);
}

}  // namespace minipro
}  // namespace jaymo
