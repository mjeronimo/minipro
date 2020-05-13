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

#ifndef UTIL__LOOP_RATE_HPP_
#define UTIL__LOOP_RATE_HPP_

#include <chrono>

#include "util/units.hpp"

namespace jeronibot {
namespace util {

class LoopRate
{
public:
  explicit LoopRate(units::frequency::hertz_t frequency);
  LoopRate() = delete;

  void sleep();

protected:
  std::chrono::steady_clock::time_point now_;
  std::chrono::steady_clock::time_point prev_;

  std::chrono::milliseconds period_{0};
};

}  // namespace util
}  // namespace jeronibot

#endif  // UTIL__LOOP_RATE_HPP_
