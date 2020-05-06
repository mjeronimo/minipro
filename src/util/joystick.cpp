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
#include <stdio.h>
#include <unistd.h>
#include <linux/joystick.h>

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
    throw("Could not open joystick");
  }

  input_thread_ = std::make_unique<std::thread>(std::bind(&Joystick::input_thread_func, this));
}

Joystick::Joystick()
: Joystick("/dev/input/js0")
{
}

Joystick::~Joystick()
{
  printf("~Joystick\n");

  // TODO(mjeronimo): input_thread_.join();
  close(fd_);
}

void
Joystick::input_thread_func()
{
  struct ::js_event event;
  struct axis_state axes[3] = {0};

  while (read(fd_, &event, sizeof(event)) == sizeof(event)) {
    switch (event.type) {
      case JS_EVENT_BUTTON:
        // printf("Button %u %s\n", event.number, event.value ? "pressed" : "released");
        if (event.number == 1) {
          printf("Button %u %s\n", event.number, event.value ? "pressed" : "released");
          axis0_.store(0);
          axis1_.store(0);
        }
        break;

      case JS_EVENT_AXIS: {
          size_t axis = get_axis_state(&event, axes);
          switch (axis) {
            case 0:
              // printf("Axis 0 - x: %6d\n", axes[axis].x);
              // printf("Axis 0 - y: %6d\n", axes[axis].y);
              axis0_.store(axes[axis].y);
              break;
            case 1:
              // printf("Axis 1 - x: %6d\n", axes[axis].x);
              // printf("Axis 2 - y: %6d\n", axes[axis].y);
              axis1_.store(axes[axis].x);
              break;
            case 2:
              // printf("Axis 2 - x: %6d\n", axes[axis].x);
              // printf("Axis 2 - y: %6d\n", axes[axis].y);
              break;
            case 3:
              // printf("Axis 3 - x: %6d\n", axes[axis].x);
              // printf("Axis 3 - y: %6d\n", axes[axis].y);
              break;
            default:
              break;
          }
          break;
        }

      default:
        break;
    }

    fflush(stdout);
  }
}

size_t
Joystick::get_axis_count()
{
  __u8 num_axes;
  if (ioctl(fd_, JSIOCGAXES, &num_axes) == -1) {
    throw("joystock ioctl (JSIOCGAXES) failed");
  }

  return num_axes;
}

size_t
Joystick::get_button_count()
{
  __u8 num_buttons;
  if (ioctl(fd_, JSIOCGBUTTONS, &num_buttons) == -1) {
    throw("joystock ioctl (JSIOCGBUTTONS) failed");
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
