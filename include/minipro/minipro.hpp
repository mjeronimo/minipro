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

#ifndef MINIPRO__MINIPRO_HPP_
#define MINIPRO__MINIPRO_HPP_

#include <cstdint>
#include <string>
#include <vector>

#include "bluetooth/bluetooth_le_client.hpp"
#include "minipro/packet.hpp"
#include "util/units.hpp"

namespace jeronibot
{
namespace minipro
{

class MiniPro : public bluetooth::BluetoothLEClient
{
public:
  explicit MiniPro(const std::string & bt_address);
  MiniPro() = delete;

  units::velocity::miles_per_hour_t get_current_speed();
  units::current::ampere_t get_battery_level();
  units::voltage::volt_t get_voltage();
  units::temperature::fahrenheit_t get_vehicle_temperature();

  void enable_notifications();
  void disable_notifications();

  void enter_remote_control_mode();
  void drive(int16_t throttle, int16_t steering);
  void exit_remote_control_mode();

protected:
  void send_packet(packet::Packet & packet);
  const uint16_t tx_service_handle_{0x00e};
};

}  // namespace minipro
}  // namespace jeronibot

#endif  // MINIPRO__MINIPRO_HPP_
