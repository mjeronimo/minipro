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

#include "minipro/enter_remote_control.hpp"

namespace jeronibot
{
namespace minipro
{
namespace packet
{

EnterRemoteControl::EnterRemoteControl()
{
  data_.push_back(0x01);
  length_ = 0x04;
}

}  // namespace packet
}  // namespace minipro
}  // namespace jeronibot
