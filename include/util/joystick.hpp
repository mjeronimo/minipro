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

#ifndef UTIL_JOYSTICK_HPP_
#define UTIL_JOYSTICK_HPP_

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include <linux/joystick.h>

namespace minipro {
namespace util {

class Joystick
{
public:
  Joystick(const std::string & device_name);
  Joystick();
  ~Joystick();

  size_t get_axis_count();
  size_t get_button_count();

  short get_axis_0() { return axis0_.load(); }
  short get_axis_1() { return axis1_.load(); }

private:
  int fd_;
  struct axis_state { short x; short y; };

  size_t get_axis_state(struct ::js_event *event, struct axis_state axes[3]);

  void input_thread_func();
  std::unique_ptr<std::thread> input_thread_;

  std::atomic<short> axis0_;
  std::atomic<short> axis1_;
};

}}

#endif  // UTIL_JOYSTICK_HPP_
