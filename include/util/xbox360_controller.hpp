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

#ifndef UTIL_XBOX360_CONTROLLER_HPP_
#define UTIL_XBOX360_CONTROLLER_HPP_

#include <functional>

#include "util/joystick.hpp"

namespace jeronibot
{
namespace util
{

class XBox360Controller : public Joystick
{
public:
  XBox360Controller(const std::string & device_name);
  XBox360Controller();

  static const uint8_t Button_A = 0;
  static const uint8_t Button_B = 1;
  static const uint8_t Button_X = 2;
  static const uint8_t Button_Y = 3;
  static const uint8_t Button_LeftShoulder = 4;
  static const uint8_t Button_RightShoulder = 5;
  static const uint8_t Button_Back = 6;
  static const uint8_t Button_Start = 7;
  static const uint8_t Button_XBox = 8;
  static const uint8_t Button_LeftThumbstick = 9;
  static const uint8_t Button_RightThumbstick = 10;

  static const uint8_t Axis_LeftThumbstick = 0;
  static const uint8_t Axis_RightThumbstick = 1;
  static const uint8_t Axis_Triggers = 2;
  static const uint8_t Axis_Digipad = 3;
};

}  // namespace util
}  // namespace jeronibot

#endif  // UTIL_XBOX360_CONTROLLER_HPP_
