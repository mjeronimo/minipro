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

#include "util/loop_rate.hpp"

namespace jaymo
{
namespace util
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

  // Set the handle to non-blocking so that the input thread
  // can break out of its read loop when shutting down
  int flags = fcntl(fd_, F_GETFL, 0);
  fcntl(fd_, F_SETFL, flags | O_NONBLOCK);

  // Launch a separate thread to handle the joystick input
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

size_t
Joystick::get_axis_state(struct ::js_event * event, AxisState axes[3])
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

void
Joystick::input_thread_func()
{
  struct ::js_event event;
  AxisState axes[3] = {0};

  jaymo::util::LoopRate loop_rate(30_Hz);

  while (!should_exit_) {
    if (read(fd_, &event, sizeof(event)) == sizeof(event)) {
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

    loop_rate.sleep();
  }
}

}  // namespace util
}  // namespace jaymo
