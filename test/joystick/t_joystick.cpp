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

#include <csignal>
#include <iostream>
#include <stdexcept>

#include "util/xbox360_controller.hpp"
#include "util/loop_rate.hpp"

using jeronibot::util::XBox360Controller;
using jeronibot::util::LoopRate;
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

    XBox360Controller controller;
    LoopRate loop_rate(30_Hz);

    controller.set_button_callback(
      XBox360Controller::Button_X,
      [](bool pressed) {std::cout << "Button X: " << pressed << std::endl;});

    controller.set_button_callback(
      XBox360Controller::Button_B,
      [](bool pressed) {std::cout << "Button B: " << pressed << std::endl;});

    while (!should_exit) {
      for (int i = 0; i < controller.get_num_axes(); i++) {
        auto x = controller.get_axis_state(i).x;
        auto y = controller.get_axis_state(i).y;

        std::cout << "x_" << i << ",y_" << i << ": " << x << "," << y << "\n";
      }

      std::cout << std::endl;
      loop_rate.sleep();
    } 
  } catch (std::exception & ex) {
    std::cerr << "Exception: " << ex.what() << std::endl;
    return -1;
  }

  return 0;
}
