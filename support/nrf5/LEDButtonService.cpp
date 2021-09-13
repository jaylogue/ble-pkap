/**
 * Copyright (c) 2020 Jay Logue
 * All rights reserved.
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
 *
 */

/**
 *   @file
 *         An implementation of the Nordic LED-Button BLE service for use with the Nordic
 *         SoftDevice and nRF5 SDK.
 */

#include <sdk_common.h>

#if defined(SOFTDEVICE_PRESENT) && SOFTDEVICE_PRESENT

#include <inttypes.h>

#include <ble.h>
#include <ble_conn_state.h>
#include <app_button.h>

#if NRF_LOG_ENABLED
#include <nrf_log.h>
#endif // NRF_LOG_ENABLED

#include <LEDButtonService.h>
#include <nRF5Utils.h>
#include <FunctExitUtils.h>

namespace nrf5utils {

namespace {

const ble_uuid128_t sServiceUUID128    = { { 0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15, 0xDE, 0xEF, 0x12, 0x12, 0x23, 0x15, 0x00, 0x00 } };
const ble_uuid128_t sButtonCharUUID128 = { { 0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15, 0xDE, 0xEF, 0x12, 0x12, 0x24, 0x15, 0x00, 0x00 } };
const ble_uuid128_t sLEDCharUUID128    = { { 0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15, 0xDE, 0xEF, 0x12, 0x12, 0x25, 0x15, 0x00, 0x00 } };

ble_uuid_t sServiceUUID;
ble_uuid_t sLEDCharUUID;
ble_uuid_t sButtonCharUUID;
uint16_t sServiceHandle;
ble_gatts_char_handles_t sLEDCharHandles;
ble_gatts_char_handles_t sButtonCharHandles;

} // unnamed namespace


ret_code_t LEDButtonService::Init(void)
{
    ret_code_t res;
    ble_gatts_attr_t attr;
    ble_gatts_attr_md_t attrMD, cccdAttrMD;
    ble_gatts_char_md_t charMD;
    uint8_t zero = 0;

    // Static declaration of BLE observer for LEDButtonService class.
    NRF_SDH_BLE_OBSERVER(sLEDButtonService_BLEObserver, LED_BUTTON_SERVICE_OBSERVER_PRIO, LEDButtonService::HandleBLEEvent, NULL);

    NRF_LOG_INFO("Adding LED-Button service");

    // Register vendor-specific UUIDs
    //     NOTE: An NRF_ERROR_NO_MEM here means the soft device hasn't been configured
    //     with space for enough custom UUIDs.  Typically, this limit is set by overriding
    //     the NRF_SDH_BLE_VS_UUID_COUNT config option.
    res = RegisterVendorUUID(sServiceUUID, sServiceUUID128);
    SuccessOrExit(res);
    res = RegisterVendorUUID(sButtonCharUUID, sButtonCharUUID128);
    SuccessOrExit(res);
    res = RegisterVendorUUID(sLEDCharUUID, sLEDCharUUID128);
    SuccessOrExit(res);

    // Add service
    res = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &sServiceUUID, &sServiceHandle);
    NRF_LOG_CALL_FAIL_INFO("sd_ble_gatts_service_add", res);
    SuccessOrExit(res);

    // Add button characteristic
    memset(&attr, 0, sizeof(attr));
    attr.p_uuid = &sButtonCharUUID;
    attr.p_attr_md = &attrMD;
    attr.max_len = 1;
    attr.init_len = 1;
    attr.p_value = &zero;
    memset(&attrMD, 0, sizeof(attrMD));
    attrMD.vloc = BLE_GATTS_VLOC_STACK;
    attrMD.read_perm = LED_BUTTON_SERVICE_CHAR_PERM;
    memset(&charMD, 0, sizeof(charMD));
    charMD.char_props.read = 1;
    charMD.char_props.notify = 1;
    charMD.p_cccd_md  = &cccdAttrMD;
    memset(&cccdAttrMD, 0, sizeof(cccdAttrMD));
    cccdAttrMD.vloc = BLE_GATTS_VLOC_STACK;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccdAttrMD.read_perm);
    cccdAttrMD.write_perm = LED_BUTTON_SERVICE_CHAR_PERM;
    res = sd_ble_gatts_characteristic_add(sServiceHandle, &charMD, &attr, &sButtonCharHandles);
    NRF_LOG_CALL_FAIL_INFO("sd_ble_gatts_characteristic_add", res);
    SuccessOrExit(res);

    // Add LED characteristic
    memset(&attr, 0, sizeof(attr));
    attr.p_uuid = &sLEDCharUUID;
    attr.p_attr_md = &attrMD;
    attr.max_len = 1;
    attr.init_len = 1;
    attr.p_value = &zero;
    memset(&attrMD, 0, sizeof(attrMD));
    attrMD.vloc = BLE_GATTS_VLOC_STACK;
    attrMD.read_perm = LED_BUTTON_SERVICE_CHAR_PERM;
    attrMD.write_perm = LED_BUTTON_SERVICE_CHAR_PERM;
    memset(&charMD, 0, sizeof(charMD));
    charMD.char_props.read = 1;
    charMD.char_props.write = 1;
    res = sd_ble_gatts_characteristic_add(sServiceHandle, &charMD, &attr, &sLEDCharHandles);
    NRF_LOG_CALL_FAIL_INFO("sd_ble_gatts_characteristic_add", res);
    SuccessOrExit(res);

exit:
    return res;
}

void LEDButtonService::UpdateButtonState(bool isPressed)
{
    // For each active connection, generate a notification for the Button characteristic
    // conveying the current state of the button.
    ble_conn_state_for_each_connected(
        [](uint16_t conHandle, void *context)
        {
            ret_code_t res;
            ble_gatts_hvx_params_t hvxParams;
            uint8_t charValue = *((bool *)context) ? 0x01 : 0x00;
            uint16_t charValueLen = 1;

            memset(&hvxParams, 0, sizeof(hvxParams));
            hvxParams.type = BLE_GATT_HVX_NOTIFICATION;
            hvxParams.handle = sButtonCharHandles.value_handle;
            hvxParams.p_data = &charValue;
            hvxParams.p_len = &charValueLen;

            res = sd_ble_gatts_hvx(conHandle, &hvxParams);
            NRF_LOG_CALL_FAIL_INFO("sd_ble_gatts_hvx", res);
        },
        &isPressed);
}

void LEDButtonService::ButtonEventHandler(uint8_t buttonPin, uint8_t buttonAction)
{
    NRF_LOG_INFO("Button state change: %s", buttonAction == APP_BUTTON_PUSH ? "PRESSED" : "RELEASED");

    UpdateButtonState(buttonAction == APP_BUTTON_PUSH);
}

void LEDButtonService::GetServiceUUID(ble_uuid_t & serviceUUID)
{
    serviceUUID = sServiceUUID;
}

void LEDButtonService::HandleBLEEvent(ble_evt_t const * bleEvent, void * context)
{
    switch (bleEvent->header.evt_id)
    {
    case BLE_GATTS_EVT_WRITE:

        // If the LED state is being written, invoke the application's OnLEDWrite
        // event handler (if defined).
        if (bleEvent->evt.gatts_evt.params.write.handle == sLEDCharHandles.value_handle &&
            bleEvent->evt.gatts_evt.params.write.len == 1)
        {
            bool setOn = (bleEvent->evt.gatts_evt.params.write.data[0] != 0);
            NRF_LOG_INFO("LED characteristic write: %s", setOn ? "ON" : "OFF");
            if (Event::OnLEDWrite)
            {
                Event::OnLEDWrite(setOn);
            }
        }

        break;

    default:
        break;
    }
}

} // namespace nrf5utils

#endif // defined(SOFTDEVICE_PRESENT) && SOFTDEVICE_PRESENT
