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

#include "minipro/enter_remote_control_mode.hpp"

#include <netinet/in.h>

namespace jeronibot
{
namespace minipro
{
namespace packet
{

EnterRemoteControlMode::EnterRemoteControlMode()
{
  type_ = Command;
  operation_ = ControlDriveBase;
  parameter_ = EnableRemoteControl;

  uint16_t enable = htons(0x0001);
  uint8_t * p = (uint8_t *) &enable;

  payload_.push_back(*p++);
  payload_.push_back(*p);
}

}  // namespace packet
}  // namespace minipro
}  // namespace jeronibot
