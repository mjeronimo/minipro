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

#include "minipro/packet.hpp"

#include <netinet/in.h>

namespace jeronibot::minipro::packet
{

Packet::Packet(const uint8_t type, const uint8_t operation, const uint8_t parameter)
: type_(type), operation_(operation), parameter_(parameter)
{
}

std::vector<uint8_t>
Packet::get_bytes()
{
  // Calculate the packet's length (includes payload and checksum) and
  // checksum (includes length, type, operation, and parameter)
  length_ = payload_.size() + sizeof(checksum_);

  uint16_t sum = length_ + type_ + operation_ + parameter_;
  for (uint8_t val : payload_) {
    sum += val;
  }
  uint16_t checksum = sum ^ 0xffff;

  // Compose the packet in a vector of bytes and return the result
  std::vector<uint8_t> bytes;

  uint16_t header = htons(header_);
  uint8_t *p = (uint8_t *) &header;
  bytes.push_back(*p++);
  bytes.push_back(*p);
  bytes.push_back(length_);
  bytes.push_back(type_);
  bytes.push_back(operation_);
  bytes.push_back(parameter_);
  for (uint8_t val : payload_) {
    bytes.push_back(val);
  }
  p = (uint8_t *) &checksum;
  bytes.push_back(*p++);
  bytes.push_back(*p);

  return bytes;
}

}  // namespace jeronibot::minipro:;packet
