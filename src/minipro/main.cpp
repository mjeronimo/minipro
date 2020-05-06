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

#include "minipro/minipro.hpp"
#include "util/joystick.hpp"
#include "util/loop_rate.hpp"

using namespace std::chrono_literals;

static std::atomic<bool> should_exit{false};

void signal_handler(int signum)
{
  should_exit = true;
}

int main(int, char **)
{
  try {
    signal(SIGINT, signal_handler);

    minipro::MiniPro minipro("F4:02:07:C6:C7:B4");
    getchar();  // TODO(mjeronimo): put a wait in the constructor

    minipro.enable_notifications();
    minipro.enter_remote_control_mode();

    minipro::util::Joystick joystick;
    minipro::util::LoopRate loop_rate(25ms);

    // Keep the MiniPRO fed with drive commands. I need to empirically 
    // test what the minimum rate is. Currently using 40Hz (25ms period)
    while (!should_exit) {
      // Flip the axis values so that forward and right are positive values
      // so that the direction of the MiniPRO matches the joysticks
      auto speed = -joystick.get_axis_0();
      auto angle = -joystick.get_axis_1();

      // Set values to zero if below a specified threshold so that the
      // MiniPRO is stable when the joysticks are released and values
      // don't go all the way back to 0
      const int zero_threshold = 4000;
      if (abs(speed) < zero_threshold) {speed = 0;}
      if (abs(angle) < zero_threshold) {angle = 0;}

      minipro.drive(speed, angle);
      loop_rate.sleep();
    }

    minipro.drive(0, 0);
    minipro.exit_remote_control_mode();
    minipro.disable_notifications();
  } catch (const char * msg) {
    // TODO(mjeronimo): update "const char *" throws in the code to specific exception
    // types derived from std::exception
  } catch (std::exception & ex) {
    // RCLCPP_ERROR(rclcpp::get_logger(node_name.c_str()), ex.what());
    // RCLCPP_ERROR(rclcpp::get_logger(node_name.c_str()), "Exiting");
    return -1;
  }

  return 0;
}
