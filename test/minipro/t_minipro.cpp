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

#include <atomic>
#include <csignal>
#include <iostream>
#include <stdexcept>

#include "minipro/minipro.hpp"
#include "util/xbox360_controller.hpp"
#include "util/loop_rate.hpp"

using jeronibot::minipro::MiniPro;
using jeronibot::util::LoopRate;
using jeronibot::util::XBox360Controller;
using units::frequency::hertz;

static std::atomic<bool> should_exit{false};

void signal_handler(int signum)
{
  should_exit = true;
}

int main(int, char **)
{
  try {
    signal(SIGINT, signal_handler);

    MiniPro minipro("F4:02:07:C6:C7:B4");
    minipro.enable_notifications();
    minipro.enter_remote_control_mode();

    XBox360Controller joystick;
    LoopRate loop_rate(30_Hz);

    while (!should_exit) {
      // Flip the axis values so that forward and right are positive values
      // so that the direction of the MiniPRO matches the joysticks
      auto throttle = -joystick.get_axis_state(XBox360Controller::Axis_LeftThumbstick).y;
      auto steering = -joystick.get_axis_state(XBox360Controller::Axis_RightThumbstick).x;

      // Set values to zero if below a specified threshold so that the MiniPRO
      // is stable when the joysticks are released (and wouldn't otherwise go
      // all the way back to 0). 4000 seems to work pretty well for my joystick
      const int zero_threshold = 4000;
      if (abs(throttle) < zero_threshold) {throttle = 0;}
      if (abs(steering) < zero_threshold) {steering = 0;}

      // Keep the MiniPRO fed with drive commands, throttling to achieve a
      // consistent rate. I need to empirically determine the minimum rate
      minipro.drive(throttle, steering);
      loop_rate.sleep();
    }

    // When exiting, make sure to stop the miniPRO and return to normal mode
    minipro.drive(0, 0);
    minipro.exit_remote_control_mode();
    minipro.disable_notifications();

  } catch (std::exception & ex) {
    std::cerr << "Exception: " << ex.what() << std::endl;
    return -1;
  }

  return 0;
}
