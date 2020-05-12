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
#include <memory>
#include <string>
#include <thread>

namespace jaymo {
namespace util {

class Joystick
{
public:
  explicit Joystick(const std::string & device_name);
  Joystick();
  ~Joystick();

  typedef struct AxisState { short x; short y; } AxisState;

  size_t get_num_axes() { return num_axes_; };
  size_t get_num_buttons() { return num_buttons_; };

  short get_axis_0() { return axis0_.load(); }
  short get_axis_1() { return axis1_.load(); }

protected:
  int fd_{-1};

  uint8_t num_axes_{0};
  uint8_t num_buttons_{0};

  size_t get_axis_state(struct ::js_event *event, AxisState axes[3]);

  void input_thread_func();
  std::unique_ptr<std::thread> input_thread_;

  std::atomic<short> axis0_{0};
  std::atomic<short> axis1_{0};

  std::atomic<bool> should_exit_{false};
};

}  // namespace util
}  // namespace jaymo

#endif  // UTIL__JOYSTICK_HPP_
