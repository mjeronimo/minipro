// Copyright (c) 2020 Michael Jeronimo
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, softwars
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <unistd.h>

#include <stdexcept>

#include "bluez.h"
#include "bluetooth/l2_cap_socket.hpp"

namespace bluetooth
{

L2CapSocket::L2CapSocket(bdaddr_t * src, bdaddr_t * dst, uint8_t dst_type, int sec)
{
  int sock = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
  if (sock < 0) {
    throw std::runtime_error("L2CapSocket: Failed to create socket");
  }

  struct sockaddr_l2 srcaddr;
  memset(&srcaddr, 0, sizeof(srcaddr));
  srcaddr.l2_family = AF_BLUETOOTH;
  srcaddr.l2_cid = htobs(ATT_CID);
  srcaddr.l2_bdaddr_type = 0;
  bacpy(&srcaddr.l2_bdaddr, src);

  if (bind(sock, (struct sockaddr *) &srcaddr, sizeof(srcaddr)) < 0) {
    close(sock);
    throw std::runtime_error("L2CapSocket: Failed to bind socket");
  }

  struct bt_security btsec;
  memset(&btsec, 0, sizeof(btsec));
  btsec.level = sec;

  if (setsockopt(sock, SOL_BLUETOOTH, BT_SECURITY, &btsec, sizeof(btsec)) != 0) {
    close(sock);
    throw std::runtime_error("L2CapSocket: Failed to set security level");
  }

  struct sockaddr_l2 dstaddr;
  memset(&dstaddr, 0, sizeof(dstaddr));
  dstaddr.l2_family = AF_BLUETOOTH;
  dstaddr.l2_cid = htobs(ATT_CID);
  dstaddr.l2_bdaddr_type = dst_type;
  bacpy(&dstaddr.l2_bdaddr, dst);

  if (connect(sock, (struct sockaddr *) &dstaddr, sizeof(dstaddr)) < 0) {
    close(sock);
    throw std::runtime_error("L2CapSocket: Failed to connect socket");
  }

  fd_ = sock;
}

L2CapSocket::~L2CapSocket()
{
}

}  // namespace bluetooth
