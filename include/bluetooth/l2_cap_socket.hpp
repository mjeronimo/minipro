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

#ifndef BLUETOOTH__L2_CAP_SOCKET_HPP_
#define BLUETOOTH__L2_CAP_SOCKET_HPP_

#include <string>

extern "C" {
#include "att.h"
#include "bluetooth.h"
#include "uuid.h"
#include "gatt-db.h"
#include "gatt-client.h"
}

namespace bluetooth {

class L2CapSocket
{
public:
  L2CapSocket(bdaddr_t * src, bdaddr_t * dst, uint8_t dst_type = BDADDR_LE_RANDOM, int sec = BT_SECURITY_LOW);
  ~L2CapSocket();

  int get_handle() { return fd_; }

protected:
  int fd_{-1};
  const int ATT_CID{4};
};

}  // namespace bluetooth

#endif  // BLUETOOTH__L2_CAP_SOCKET_HPP_

