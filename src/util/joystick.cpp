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

#include "util/joystick.hpp"

#include <fcntl.h>
#include <linux/joystick.h>
#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <string>

#include "util/loop_rate.hpp"

namespace jeronibot::util
{

Joystick::Joystick(const std::string & device_name)
{
  if ((fd_ = open(device_name.c_str(), O_RDONLY)) == -1) {
    throw std::runtime_error("Joystick: Couldn't open joystick device");
  }

  if (ioctl(fd_, JSIOCGAXES, &num_axes_) == -1) {
    throw std::runtime_error("Joystick: ioctl (JSIOCGAXES) failed");
  }

  if (ioctl(fd_, JSIOCGBUTTONS, &num_buttons_) == -1) {
    throw std::runtime_error("Joystick: ioctl (JSIOCGBUTTONS) failed");
  }

  for (int i = 0; i < num_axes_; i++) {
    axis_map_[i].x = 0;
    axis_map_[i].y = 0;
  }

  for (int i = 0; i < num_buttons_; i++) {
    button_map_[i] = nullptr;
  }

  // Set the handle to non-blocking so that the input thread
  // can break out of its read loop when shutting down
  int current_flags = fcntl(fd_, F_GETFL, 0);
  fcntl(fd_, F_SETFL, current_flags | O_NONBLOCK);

  // Launch a separate thread to handle the joystick input
  input_thread_ = std::make_unique<std::thread>(std::bind(&Joystick::input_thread_func, this));
}

Joystick::Joystick()
: Joystick("/dev/input/js0")
{
}

Joystick::~Joystick()
{
  should_exit_.store(true);
  input_thread_->join();
  close(fd_);
}

AxisState
Joystick::get_axis_state(uint8_t axis)
{
  if (axis >= num_axes_) {
    throw std::runtime_error("Joystick: get_axis_state: axis value out of range");
  }

  return {axis_map_[axis].x, axis_map_[axis].y};
}

void
Joystick::set_button_callback(uint8_t button, std::function<void(bool)> callback)
{
  if (button >= num_buttons_) {
    throw std::runtime_error("Joystick: set_button_callback: button value out of range");
  }

  button_map_[button] = callback;
}

void
Joystick::input_thread_func()
{
  struct ::js_event event;
  jeronibot::util::LoopRate loop_rate(60_Hz);

  while (!should_exit_) {
    if (read(fd_, &event, sizeof(event)) == sizeof(event)) {
      switch (event.type) {
        case JS_EVENT_BUTTON:
          // printf("Button %u %s\n", event.number, event.value ? "pressed" : "released");
          if (button_map_[event.number] != nullptr) {
            button_map_[event.number](event.value ? true : false);
          }
          break;

        case JS_EVENT_AXIS:
          {
            // Each axis has two event numbers (for x and y)
            size_t axis = event.number / 2;

            // The first event number is x and the second is y
            if (event.number % 2 == 0) {
              axis_map_[axis].x = event.value;
            } else {
              axis_map_[axis].y = event.value;
            }
          }
          break;

        default:
          break;
      }
    }

    loop_rate.sleep();
  }
}

}  // namespace jeronibot::util
