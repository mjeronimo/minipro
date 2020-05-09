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
#include <unistd.h>
#include <linux/joystick.h>

#include <atomic>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <string>

namespace minipro
{
namespace util
{

Joystick::Joystick(const std::string & device_name)
{
  if ((fd_ = open(device_name.c_str(), O_RDONLY)) == -1) {
    throw std::runtime_error("Could not open joystick");
  }

  int flags = fcntl(fd_, F_GETFL, 0);
  fcntl(fd_, F_SETFL, flags | O_NONBLOCK);

  input_thread_ = std::make_unique<std::thread>(std::bind(&Joystick::input_thread_func, this));
}

Joystick::Joystick()
: Joystick("/dev/input/js0")
{
}

Joystick::~Joystick()
{
  printf("~Joystick\n");
  should_exit_.store(true);
  input_thread_->join();
  close(fd_);
}

void
Joystick::input_thread_func()
{
  struct ::js_event event;
  struct axis_state axes[3] = {0};

  while (!should_exit_) {
    auto rc = read(fd_, &event, sizeof(event));

    if (rc == -1 && errno == EAGAIN) {
      continue;
    } else if (rc == sizeof(event)) {
      switch (event.type) {
        case JS_EVENT_BUTTON:
          // printf("Button %u %s\n", event.number, event.value ? "pressed" : "released");
          if (event.number == 1) {
            axis0_.store(0);
            axis1_.store(0);
          }
          break;
  
        case JS_EVENT_AXIS: {
            size_t axis = get_axis_state(&event, axes);
            switch (axis) {
              case 0:
                axis0_.store(axes[axis].y);
                break;

              case 1:
                axis1_.store(axes[axis].x);
                break;

              default:
                break;
            }
            break;
          }
  
        default:
          break;
      }
	  }
  }
}

size_t
Joystick::get_axis_count()
{
  uint8_t num_axes = 0;;
  if (ioctl(fd_, JSIOCGAXES, &num_axes) == -1) {
    throw std::runtime_error("Joystick::get_axis_count: ioctl (JSIOCGAXES) failed");
  }
  return num_axes;
}

size_t
Joystick::get_button_count()
{
  uint8_t num_buttons = 0;
  if (ioctl(fd_, JSIOCGBUTTONS, &num_buttons) == -1) {
    throw std::runtime_error("Joystick::get_button_count: ioctl (JSIOCGBUTTONS) failed");
  }
  return num_buttons;
}

size_t
Joystick::get_axis_state(struct ::js_event * event, struct axis_state axes[3])
{
  size_t axis = event->number / 2;
  if (axis < 3) {
    if (event->number % 2 == 0) {
      axes[axis].x = event->value;
    } else {
      axes[axis].y = event->value;
    }
  }
  return axis;
}

}  // namespace util
}  // namespace minipro
