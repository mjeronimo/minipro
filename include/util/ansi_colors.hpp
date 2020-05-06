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

#ifndef UTIL_ANSI_COLORS_HPP_
#define UTIL_ANSI_COLORS_HPP_

namespace minipro {
namespace util {

#define COLOR_OFF   	"\x1B[0m"
#define COLOR_RED   	"\x1B[0;91m"
#define COLOR_GREEN 	"\x1B[0;92m"
#define COLOR_YELLOW    "\x1B[0;93m"
#define COLOR_BLUE  	"\x1B[0;94m"
#define COLOR_MAGENTA   "\x1B[0;95m"
#define COLOR_BOLDGRAY  "\x1B[1;30m"
#define COLOR_BOLDWHITE "\x1B[1;36m"

}}

#endif  // UTIL_ANSI_COLORS_HPP_
