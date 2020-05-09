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

namespace minipro
{
namespace util
{

LoopRate::LoopRate(units::frequency::hertz_t hz)
{
  std::chrono::milliseconds one_second{1000};
  auto period_value = one_second.count() / units::unit_cast<int>(hz);

  if (std::isinf(period_value) || std::isnan(period_value)) {
    throw std::runtime_error("LoopRate: invalid frequency specified, resulting in divide-by-zero");
  }

  period_ = std::chrono::milliseconds(period_value);

  t1_ = std::chrono::steady_clock::now();
  t2_ = std::chrono::steady_clock::now();
}

void
LoopRate::sleep()
{
  t1_ = std::chrono::steady_clock::now();
  std::chrono::duration<double, std::milli> work_time = t1_ - t2_;

  if (work_time < period_) {
    std::chrono::duration<double, std::milli> delta_ms(period_ - work_time);
    std::this_thread::sleep_for(delta_ms);
  }

  t2_ = std::chrono::steady_clock::now();
  std::chrono::duration<double, std::milli> sleep_time = t2_ - t1_;

  // printf("Time: %f \n", (work_time + sleep_time).count());
}

}  // namespace util
}  // namespace minipro
