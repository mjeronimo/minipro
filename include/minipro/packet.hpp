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

#ifndef MINIPRO__MINIPRO_PACKET_HPP_
#define MINIPRO__MINIPRO_PACKET_HPP_

#include <cstdint>
#include <vector>

namespace jeronibot::minipro::packet
{

class Packet
{
public:
  Packet(const uint8_t type, const uint8_t operation, const uint8_t parameter);

  std::vector<uint8_t> get_bytes();

protected:
  enum packet_type : uint8_t { Command = 0xa, Notification = 0xd };
  enum operation : uint8_t { GetSetValue = 0x01, ControlDriveBase = 0x03 };
  enum parameter : uint8_t { EnableRemoteControl = 0x7a, SetDrive = 0x7b };

  const uint16_t header_{ 0x55aa };
  uint8_t length_{0};
  const uint8_t type_{0};
  const uint8_t operation_{0};
  const uint8_t parameter_{0};
  std::vector<uint8_t> payload_;
  uint16_t checksum_{0};
};

}  // namespace jeronibot::minipro::packet

#endif  // MINIPRO__MINIPRO_PACKET_HPP_
