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

#include "util/loop_rate.hpp"

#include <chrono>
#include <cmath>
#include <exception>
#include <thread>

namespace jeronibot
{
namespace util
{

LoopRate::LoopRate(units::frequency::hertz_t hz)
{
  const std::chrono::milliseconds one_second{1000};
  auto period_value = one_second.count() / units::unit_cast<int>(hz);

  if (std::isinf(period_value) || std::isnan(period_value)) {
    throw std::runtime_error("LoopRate: divide-by-zero: invalid frequency specified");
  }

  period_ = std::chrono::milliseconds(period_value);

  now_ = std::chrono::steady_clock::now();
  prev_ = std::chrono::steady_clock::now();
}

void
LoopRate::sleep()
{
  now_ = std::chrono::steady_clock::now();
  std::chrono::duration<double, std::milli> work_time = now_ - prev_;

  if (work_time < period_) {
    std::chrono::duration<double, std::milli> delta_ms(period_ - work_time);
    std::this_thread::sleep_for(delta_ms);
  }

  prev_ = std::chrono::steady_clock::now();
}

}  // namespace util
}  // namespace jeronibot
