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

#ifndef UTIL__JOYSTICK_HPP_
#define UTIL__JOYSTICK_HPP_

#include <linux/joystick.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>

namespace jeronibot::util
{

typedef struct AxisState {
  int16_t x; 
  int16_t y; 
} AxisState;

class Joystick
{
public:
  explicit Joystick(const std::string & device_name);
  Joystick();

  ~Joystick();

  uint8_t get_num_axes() { return num_axes_; };
  uint8_t get_num_buttons() { return num_buttons_; };

  AxisState get_axis_state(uint8_t axis);
  void set_button_callback(uint8_t button, std::function<void(bool)> callback);

protected:
  int fd_{-1};

  uint8_t num_axes_{0};
  uint8_t num_buttons_{0};

  struct XY { std::atomic<int16_t> x; std::atomic<int16_t> y; };
  std::map<uint8_t, struct XY> axis_map_;

  std::map<uint8_t, std::function<void(int)>> button_map_;

  void input_thread_func();
  std::atomic<bool> should_exit_{false};
  std::unique_ptr<std::thread> input_thread_;
};

}  // namespace jeronibot::util

#endif  // UTIL__JOYSTICK_HPP_
