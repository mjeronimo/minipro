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
utils::ecode_to_string(uint8_t ecode)
{
  switch (ecode) {
    case BT_ATT_ERROR_ATTRIBUTE_NOT_FOUND: return "Attribute Not Found";
    case BT_ATT_ERROR_ATTRIBUTE_NOT_LONG: return "Attribute Not Long";
    case BT_ATT_ERROR_AUTHENTICATION: return "Authentication Required";
    case BT_ATT_ERROR_AUTHORIZATION: return "Authorization Required";
    case BT_ATT_ERROR_INSUFFICIENT_ENCRYPTION_KEY_SIZE: return "Insuficient Encryption Key Size";
    case BT_ATT_ERROR_INSUFFICIENT_ENCRYPTION: return "Insufficient Encryption";
    case BT_ATT_ERROR_INSUFFICIENT_RESOURCES: return "Insufficient Resources";
    case BT_ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LEN: return "Invalid Attribute value len";
    case BT_ATT_ERROR_INVALID_HANDLE: return "Invalid Handle";
    case BT_ATT_ERROR_INVALID_OFFSET: return "Invalid Offset";
    case BT_ATT_ERROR_INVALID_PDU: return "Invalid PDU";
    case BT_ATT_ERROR_PREPARE_QUEUE_FULL: return "Prepare Write Queue Full";
    case BT_ATT_ERROR_READ_NOT_PERMITTED: return "Read Not Permitted";
    case BT_ATT_ERROR_REQUEST_NOT_SUPPORTED: return "Request Not Supported";
    case BT_ATT_ERROR_UNLIKELY: return "Unlikely Error";
    case BT_ATT_ERROR_UNSUPPORTED_GROUP_TYPE: return "Group type Not Supported";
    case BT_ATT_ERROR_WRITE_NOT_PERMITTED: return "Write Not Permitted";
    case BT_ERROR_ALREADY_IN_PROGRESS: return "Procedure Already in Progress";
    case BT_ERROR_CCC_IMPROPERLY_CONFIGURED: return "CCC Improperly Configured";
    case BT_ERROR_OUT_OF_RANGE: return "Out of Range";
    default:
      return "Unknown error type";
  }
}

}  // namespace bluetooth
