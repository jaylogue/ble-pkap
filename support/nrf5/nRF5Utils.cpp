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

#include <stdint.h>
#include <inttypes.h>
#include <malloc.h>

#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"

#if NRF_LOG_ENABLED
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#endif // NRF_LOG_ENABLED

#include <nRF5Utils.h>
#include <FunctExitUtils.h>

namespace nrf5utils {

#if defined(SOFTDEVICE_PRESENT) && SOFTDEVICE_PRESENT

ret_code_t RegisterVendorUUID(ble_uuid_t & uuid, const ble_uuid128_t & vendorUUID)
{
    ret_code_t res;
    ble_uuid128_t vendorBaseUUID;
    uint8_t shortUUIDType;

    // Construct a "base" UUID from the given vendor UUID by zeroing out bytes 2 and 3.
    //     NOTE: the SoftDevice API expects UUIDs in little-endian form, so UUID bytes 2 and 3
    //     correspond to offsets 13 and 12 in the array.
    vendorBaseUUID = vendorUUID;
    vendorBaseUUID.uuid128[13] = vendorBaseUUID.uuid128[12] = 0;

    // Register the base UUID with the SoftDevice and get the corresponding "short" UUID type.
    // By registering the base UUID value, instead of the full vendor UUID, we save space in
    // the SoftDevice UUID table by consuming only a single entry when there are multiple vendor
    // UUIDs that vary only in bytes 2 and 3.
    res = sd_ble_uuid_vs_add(&vendorBaseUUID, &shortUUIDType);
    NRF_LOG_CALL_FAIL_INFO("sd_ble_uuid_vs_add", res);
    SuccessOrExit(res);

    // Initialize the SoftDevice "short" UUID structure that will be used to refer to this vendor UUID.
    uuid.type = shortUUIDType;
    uuid.uuid = (((uint16_t)vendorUUID.uuid128[13]) << 8) | vendorUUID.uuid128[12];

exit:
    return res;
}

#endif // defined(SOFTDEVICE_PRESENT) && SOFTDEVICE_PRESENT

const char * GetSecStatusStr(uint8_t secStatus)
{
    switch (secStatus)
    {
        case BLE_GAP_SEC_STATUS_SUCCESS                : return "SUCCESS";
        case BLE_GAP_SEC_STATUS_TIMEOUT                : return "Procedure timed out";
        case BLE_GAP_SEC_STATUS_PDU_INVALID            : return "Invalid PDU received";
        case BLE_GAP_SEC_STATUS_PASSKEY_ENTRY_FAILED   : return "Passkey entry failed (user canceled or other)";
        case BLE_GAP_SEC_STATUS_OOB_NOT_AVAILABLE      : return "Out of Band Key not available";
        case BLE_GAP_SEC_STATUS_AUTH_REQ               : return "Authentication requirements not met";
        case BLE_GAP_SEC_STATUS_CONFIRM_VALUE          : return "Confirm value failed";
        case BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP       : return "Pairing not supported";
        case BLE_GAP_SEC_STATUS_ENC_KEY_SIZE           : return "Encryption key size";
        case BLE_GAP_SEC_STATUS_SMP_CMD_UNSUPPORTED    : return "Unsupported SMP command";
        case BLE_GAP_SEC_STATUS_UNSPECIFIED            : return "Unspecified reason";
        case BLE_GAP_SEC_STATUS_REPEATED_ATTEMPTS      : return "Too little time elapsed since last attempt";
        case BLE_GAP_SEC_STATUS_INVALID_PARAMS         : return "Invalid parameters";
        case BLE_GAP_SEC_STATUS_DHKEY_FAILURE          : return "DHKey check failure";
        case BLE_GAP_SEC_STATUS_NUM_COMP_FAILURE       : return "Numeric Comparison failure";
        case BLE_GAP_SEC_STATUS_BR_EDR_IN_PROG         : return "BR/EDR pairing in progress";
        case BLE_GAP_SEC_STATUS_X_TRANS_KEY_DISALLOWED : return "BR/EDR Link Key cannot be used for LE keys";
        default                                        : return "(unknown)";        
    }
}

extern "C" size_t GetHeapTotalSize(void) __WEAK;

void LogHeapStats(void)
{
#if NRF_LOG_ENABLED && NRF_LOG_LEVEL >= NRF_LOG_SEVERITY_INFO

    {
        struct mallinfo minfo = mallinfo();
        size_t totalHeapSize = GetHeapTotalSize ? GetHeapTotalSize() : 0;
        NRF_LOG_INFO("System Heap Utilization: heap size %" PRId32 ", arena size %" PRId32 ", in use %" PRId32 ", free %" PRId32,
                totalHeapSize, minfo.arena, minfo.uordblks, minfo.fordblks);
    }

#endif
}


} // namespace nrf5utils