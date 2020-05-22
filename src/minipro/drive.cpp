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

namespace jeronibot
{
namespace minipro
{
namespace packet
{

Drive::Drive()
{
  type_ = Command;
  operation_ = ControlDriveBase;
  parameter_ = SetDrive;

  for (int i=0; i<4; i++) {
    payload_.push_back(0);
  }
}

void
Drive::setThrottle(uint16_t throttle)
{
  uint16_t value = htons(throttle);
  uint8_t * p = (uint8_t *) &value;

  payload_[0] = *p++;
  payload_[1] = *p;
}

void
Drive::setSteering(uint16_t steering)
{
  uint16_t value = htons(steering);
  uint8_t * p = (uint8_t *) &value;

  payload_[3] = *p++;
  payload_[4] = *p;
}

}  // namespace packet
}  // namespace minipro
}  // namespace jeronibot
