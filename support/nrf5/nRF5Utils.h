/*
 *
 *    Copyright (c) 2021 Jay Logue
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 *   @file
 *         General utility functions for use with the nRF5 SDK.
 */

#ifndef NRF5UTILS_H_
#define NRF5UTILS_H_

#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"

namespace nrf5utils {

extern ret_code_t RegisterVendorUUID(ble_uuid_t & uuid, const ble_uuid128_t & vendorUUID);
extern const char * GetSecStatusStr(uint8_t secStatus);
extern void LogHeapStats(void);

} // namespace nrf5utils

/** Enable the logging of function call failures.
 */
#ifndef NRF_LOG_CALL_FAIL
#define NRF_LOG_CALL_FAIL 1
#endif // NRF_LOG_CALL_FAIL

#if NRF_LOG_CALL_FAIL

/** Log the result of a function call if it failed
 */
#define NRF_LOG_CALL_FAIL_ERROR(FUNCT_NAME, RES)                        \
do                                                                      \
{                                                                       \
    if (RES != NRF_SUCCESS)                                             \
    {                                                                   \
        NRF_LOG_ERROR("%s() failed: 0x%08" PRIX32, FUNCT_NAME, RES);    \
    }                                                                   \
} while (0)

/** Log the result of a function call if it failed
 */
#define NRF_LOG_CALL_FAIL_INFO(FUNCT_NAME, RES)                         \
do                                                                      \
{                                                                       \
    if (RES != NRF_SUCCESS)                                             \
    {                                                                   \
        NRF_LOG_INFO("%s() failed: 0x%08" PRIX32, FUNCT_NAME, RES);     \
    }                                                                   \
} while (0)

#else // NRF_LOG_CALL_FAIL

#define NRF_LOG_CALL_FAIL_ERROR(FUNCT_NAME, RES)
#define NRF_LOG_CALL_FAIL_INFO(FUNCT_NAME, RES)

#endif // NRF_LOG_CALL_FAIL

#endif // NRF5UTILS_H_
