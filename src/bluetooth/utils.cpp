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

#include "att-types.h"
#include "bluetooth/utils.hpp"

namespace bluetooth
{

const char *
utils::to_string(uint8_t ecode)
{
  switch (ecode) {
    case BT_ATT_ERROR_ATTRIBUTE_NOT_FOUND: return "Attribute not found";
    case BT_ATT_ERROR_ATTRIBUTE_NOT_LONG: return "Attribute not long";
    case BT_ATT_ERROR_AUTHENTICATION: return "Authentication required";
    case BT_ATT_ERROR_AUTHORIZATION: return "Authorization required";
    case BT_ATT_ERROR_INSUFFICIENT_ENCRYPTION_KEY_SIZE: return "Insufficient encryption key size";
    case BT_ATT_ERROR_INSUFFICIENT_ENCRYPTION: return "Insufficient encryption";
    case BT_ATT_ERROR_INSUFFICIENT_RESOURCES: return "Insufficient resources";
    case BT_ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LEN: return "Invalid attribute value length";
    case BT_ATT_ERROR_INVALID_HANDLE: return "Invalid handle";
    case BT_ATT_ERROR_INVALID_OFFSET: return "Invalid offset";
    case BT_ATT_ERROR_INVALID_PDU: return "Invalid PDU";
    case BT_ATT_ERROR_PREPARE_QUEUE_FULL: return "Prepare queue full";
    case BT_ATT_ERROR_READ_NOT_PERMITTED: return "Read not permitted";
    case BT_ATT_ERROR_REQUEST_NOT_SUPPORTED: return "Request not supported";
    case BT_ATT_ERROR_UNLIKELY: return "Unlikely error";
    case BT_ATT_ERROR_UNSUPPORTED_GROUP_TYPE: return "Group type not supported";
    case BT_ATT_ERROR_WRITE_NOT_PERMITTED: return "Write not permitted";
    case BT_ERROR_ALREADY_IN_PROGRESS: return "Already in progress";
    case BT_ERROR_CCC_IMPROPERLY_CONFIGURED: return "CCC improperly configured";
    case BT_ERROR_OUT_OF_RANGE: return "Out of range";
    default: return "Unknown error type";
  }
}

}  // namespace bluetooth
