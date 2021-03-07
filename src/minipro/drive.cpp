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

#include "minipro/drive.hpp"

#include <netinet/in.h>

namespace jeronibot::minipro::packet
{

Drive::Drive(uint16_t throttle, uint16_t steering)
: Packet(Command, ControlDriveBase, SetDrive)
{
  uint8_t * p = (uint8_t *) &throttle;
  payload_.push_back(*p++);
  payload_.push_back(*p);

  p = (uint8_t *) &steering;
  payload_.push_back(*p++);
  payload_.push_back(*p);
}

}  // namespace jeronibot::minipro::packet
