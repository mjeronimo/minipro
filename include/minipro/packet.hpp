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

namespace jeronibot
{
namespace minipro
{
namespace packet
{

class Packet
{
public:
  Packet();

  std::vector<uint8_t> get_buffer();

protected:
  void calculate_checksum();

  const uint8_t header_[2]{ 0x55, 0xaa };
  uint8_t length_{0};
  std::vector<uint8_t> data_;
  uint8_t checksum_[2]{0,0};
};

}  // namespace packet
}  // namespace minipro
}  // namespace jeronibot

#endif  // MINIPRO__MINIPRO_PACKET_HPP_
