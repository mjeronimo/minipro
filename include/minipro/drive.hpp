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

#ifndef MINIPRO__MINIPRO_DRIVE_HPP_
#define MINIPRO__MINIPRO_DRIVE_HPP_

#include <cstdint>

#include "minipro/packet.hpp"

namespace jeronibot::minipro::packet
{

class Drive : public Packet
{
public:
  Drive(uint16_t throttle, uint16_t steering);
  Drive() = delete;
};

}  // namespace jeronibot::minipro::packet

#endif  // MINIPRO__MINIPRO_DRIVE_HPP_

