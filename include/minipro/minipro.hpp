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

#ifndef MINIPRO_MINIPRO_HPP_
#define MINIPRO_MINIPRO_HPP_

#include <cstdint>
#include <string>

#include "bluetooth/bluetooth_le_device.hpp"

namespace minipro
{

class MiniPro : public bluetooth::BluetoothLEDevice
{
public:
  MiniPro(const std::string & bt_address);
  MiniPro() = delete;

  // Use units library (mph, degrees)?
  int get_current_speed();        // mph
  int get_battery_level();        // amps
  int get_voltage();              // volts
  int get_vehicle_temperature();  // degrees F

  void enable_notifications();
  void disable_notifications();

  void enter_remote_control_mode();
  void exit_remote_control_mode();

  void drive(int16_t speed, int16_t angle);
};

}  // namespace minipro

#endif // MINIPRO_MINIPRO_HPP_
