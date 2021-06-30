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
 *         A sample BLE service implementing the BLE-PKAP protocol.
 */

#include <sdk_common.h>

#if defined(SOFTDEVICE_PRESENT) && SOFTDEVICE_PRESENT

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>

#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_sdh_soc.h"
#include "ble.h"
#include "ble_gap.h"
#include "ble_gattc.h"
#include "ble_advdata.h"
#include "ble_srv_common.h"
#include "nrf_ble_gatt.h"
#include "ble_conn_state.h"
#include "nrf_ble_lesc.h"

#if NRF_LOG_ENABLED
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#endif // NRF_LOG_ENABLED

#include <BLEPKAPService.h>
#include <FunctExitUtils.h>
#include <nRF5LESCOOB.h>

namespace {

constexpr size_t kP256PubKeyCoordLength = 32;

const ble_uuid128_t sLEDButtonServiceUUID128 = { { 0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15, 0xDE, 0xEF, 0x12, 0x12, 0x23, 0x15, 0x00, 0x00 } };
const ble_uuid128_t sButtonCharUUID128       = { { 0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15, 0xDE, 0xEF, 0x12, 0x12, 0x24, 0x15, 0x00, 0x00 } };
const ble_uuid128_t sLEDCharUUID128          = { { 0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15, 0xDE, 0xEF, 0x12, 0x12, 0x25, 0x15, 0x00, 0x00 } };

const ble_uuid128_t sBLEPKAPServiceUUID128   = { { 0x24, 0x6b, 0x33, 0x15, 0x5f, 0x1c, 0x3c, 0x58, 0xc0, 0xe6, 0x2c, 0xbc, 0x00, 0xee, 0x78, 0xe2 } };
const ble_uuid128_t sBLEPKAPAuthCharUUID128  = { { 0x24, 0x6b, 0x33, 0x15, 0x5f, 0x1c, 0x3c, 0x58, 0xc0, 0xe6, 0x2c, 0xbc, 0x01, 0xee, 0x78, 0xe2 } };;

NRF_BLE_GATT_DEF(sGATTModule);
uint8_t sAdvHandle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;
uint8_t sEncodedAdvDataBuf[BLE_GAP_ADV_SET_DATA_SIZE_MAX];
uint16_t sEncodedAdvDataLen;
char sDeviceName[16];
ble_uuid_t sLEDButtonServiceUUID;
ble_uuid_t sLEDCharUUID;
ble_uuid_t sButtonCharUUID;
ble_uuid_t sBLEPKAPServiceUUID;
ble_uuid_t sBLEPKAPAuthCharUUID;
uint16_t sLEDButtonServiceHandle;
ble_gatts_char_handles_t sLEDCharHandles;
ble_gatts_char_handles_t sButtonCharHandles;
uint16_t sBLEPKAPServiceHandle;
ble_gatts_char_handles_t sBLEPKAPAuthCharHandles;

ble_gap_lesc_p256_pk_t sPeerLESCPubKey;
ble_gap_sec_keyset_t sKeySet;

struct PeerAuthData
{
    static constexpr size_t RANDOM_VALUE_LEN = BLE_GAP_SEC_KEY_LEN;
    static constexpr size_t CONFIRM_VALUE_LEN = BLE_GAP_SEC_KEY_LEN;
    static constexpr size_t TOTAL_LEN = RANDOM_VALUE_LEN + CONFIRM_VALUE_LEN;

    uint16_t conHandle;
    uint8_t randomValue[RANDOM_VALUE_LEN];
    uint8_t confirmValue[CONFIRM_VALUE_LEN];

    bool Decode(const uint8_t * buf, size_t len, uint16_t conHandle)
    {
        if (len == TOTAL_LEN)
        {
            memcpy(randomValue, buf, RANDOM_VALUE_LEN);
            memcpy(confirmValue, buf + RANDOM_VALUE_LEN, CONFIRM_VALUE_LEN);
            this->conHandle = conHandle;
        }
        else
            this->conHandle = BLE_CONN_HANDLE_INVALID;
        return (this->conHandle != BLE_CONN_HANDLE_INVALID);
    }

    void Clear()
    {
        conHandle = BLE_CONN_HANDLE_INVALID;
        memset(randomValue, 0, RANDOM_VALUE_LEN);
        memset(confirmValue, 0, CONFIRM_VALUE_LEN);
    }
} sPeerAuthData;

void ToHexString(uint8_t * data, size_t dataLen, char * outBuf, size_t outBufSize)
{
    for (; dataLen > 0; data++, dataLen--)
    {
        snprintf(outBuf, outBufSize, "%02" PRIx8, *data);
        if (outBufSize <= 2)
            break;
        outBuf += 2;
        outBufSize -= 2;
    }
}

} // unnamed namespace

ret_code_t SampleBLEService::Init(void)
{
    ret_code_t res;

    sPeerAuthData.Clear();

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, BLE_OBSERVER_PRIO, SampleBLEService::HandleBLEEvent, NULL);

    ble_conn_state_init();

    res = nrf_ble_lesc_init();
    SuccessOrExit(res);
    nrf_ble_lesc_peer_oob_data_handler_set(GetPeerLESCOOBData);

    res = SetDeviceName();
    SuccessOrExit(res);

    res = ConfigureGATTService();
    SuccessOrExit(res);

    res = ConfigureAdvertising();
    SuccessOrExit(res);

    res = StartAdvertising();
    SuccessOrExit(res);

    {
        char buf[kP256PubKeyCoordLength * 2 + 1];
        ble_gap_lesc_p256_pk_t * localPubKey = nrf_ble_lesc_public_key_get();

        NRF_LOG_INFO("Local LESC public key:");
        ToHexString(localPubKey->pk, kP256PubKeyCoordLength, buf, sizeof(buf));
        NRF_LOG_INFO("  X: %s", buf);
        ToHexString(localPubKey->pk + kP256PubKeyCoordLength, kP256PubKeyCoordLength, buf, sizeof(buf));
        NRF_LOG_INFO("  Y: %s", buf);
    }

    {
        char buf[BLE_GAP_SEC_KEY_LEN * 2 + 1];

        res = nrf_ble_lesc_own_oob_data_generate();
        SuccessOrExit(res);

        ble_gap_lesc_oob_data_t * localOOBData = nrf_ble_lesc_own_oob_data_get();

        NRF_LOG_INFO("Local LESC OOB data:");
        ToHexString(localOOBData->c, BLE_GAP_SEC_KEY_LEN, buf, sizeof(buf));
        NRF_LOG_INFO("  Confirmation Value: %s", buf);
        ToHexString(localOOBData->r, BLE_GAP_SEC_KEY_LEN, buf, sizeof(buf));
        NRF_LOG_INFO("  Random Value: %s", buf);
    }

exit:
    return res;
}

void SampleBLEService::Shutdown(void)
{

}

void SampleBLEService::UpdateButtonState(bool isPressed)
{
    // For each active connection, generate a notification for the Button characteristic.
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
            if (res != NRF_SUCCESS)
            {
                NRF_LOG_INFO("Error sending BLE notification (con %" PRIu16 "): 0x%08" PRIX32, conHandle, res);
            }
        },
        &isPressed);
}

ret_code_t SampleBLEService::SetDeviceName(void)
{
    ret_code_t res;
    ble_gap_conn_sec_mode_t secMode;
    ble_gap_addr_t devAddr;

    // Get the device's BLE MAC address
    res = sd_ble_gap_addr_get(&devAddr);
    SuccessOrExit(res);

    // Form a unique device name based on the last digits of the MAC address.
    snprintf(sDeviceName, sizeof(sDeviceName), "%.*s%02" PRIX8 "%02" PRIX8,
             sizeof(sDeviceName) - 5, APP_DEVICE_NAME_PREFIX, devAddr.addr[1], devAddr.addr[0]);
    sDeviceName[sizeof(sDeviceName)-1] = 0;

    // Do not allow device name characteristic to be changed
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&secMode);

    // Configure the device name within the BLE soft device.
    res = sd_ble_gap_device_name_set(&secMode, (const uint8_t *)sDeviceName, strlen(sDeviceName));
    SuccessOrExit(res);

exit:
    return res;
}

ret_code_t SampleBLEService::ConfigureAdvertising(void)
{
    ret_code_t res;
    ble_advdata_t advData;
    ble_gap_adv_data_t gapAdvData;
    ble_gap_adv_params_t gapAdvParams;

    // Form the contents of the advertising packet.
    memset(&advData, 0, sizeof(advData));
    advData.name_type = BLE_ADVDATA_FULL_NAME;
    advData.include_appearance = false;
    advData.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    sEncodedAdvDataLen = sizeof(sEncodedAdvDataBuf);
    res = ble_advdata_encode(&advData, sEncodedAdvDataBuf, &sEncodedAdvDataLen);
    SuccessOrExit(res);

    // Setup parameters controlling how advertising will happen.
    memset(&gapAdvParams, 0, sizeof(gapAdvParams));
    gapAdvParams.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
    gapAdvParams.primary_phy     = BLE_GAP_PHY_1MBPS;
    gapAdvParams.duration        = BLE_GAP_ADV_TIMEOUT_GENERAL_UNLIMITED;
    gapAdvParams.filter_policy   = BLE_GAP_ADV_FP_ANY;
    gapAdvParams.interval        = MSEC_TO_UNITS(APP_ADVERTISING_RATE_MS, UNIT_0_625_MS);
    gapAdvParams.duration        = 0; // No timeout

    // Configure an "advertising set" in the BLE soft device with the given data and parameters.
    // If the advertising set doesn't exist, this call will create it and return its handle.
    memset(&gapAdvData, 0, sizeof(gapAdvData));
    gapAdvData.adv_data.p_data = sEncodedAdvDataBuf;
    gapAdvData.adv_data.len = sEncodedAdvDataLen;
    res = sd_ble_gap_adv_set_configure(&sAdvHandle, &gapAdvData, &gapAdvParams);
    SuccessOrExit(res);

exit:
    return res;
}

ret_code_t SampleBLEService::StartAdvertising(void)
{
    ret_code_t res;

    VerifyOrExit(sAdvHandle != BLE_GAP_ADV_SET_HANDLE_NOT_SET, res = NRF_ERROR_INVALID_STATE);

#if NRF_LOG_ENABLED && NRF_LOG_LEVEL >= NRF_LOG_SEVERITY_INFO

    {
        ble_gap_addr_t devAddr;
        char devAddrStr[6*3+1];
        sd_ble_gap_addr_get(&devAddr);
        snprintf(devAddrStr, sizeof(devAddrStr), "%02" PRIX8 ":%02" PRIX8 ":%02" PRIX8 ":%02" PRIX8 ":%02" PRIX8 ":%02" PRIX8,
                 devAddr.addr[5], devAddr.addr[4], devAddr.addr[3], devAddr.addr[2], devAddr.addr[1], devAddr.addr[0]);
        NRF_LOG_INFO("Starting BLE advertising (device name: %s, MAC addr: %s)", sDeviceName, devAddrStr);
    }

#endif

    // Instruct the soft device to start advertising using the configured advertising set.
    res = sd_ble_gap_adv_start(sAdvHandle, BLE_CONN_CONFIG_TAG);
    SuccessOrExit(res);

    // Let other portions of the application know that advertising has started.
    OnAdvertisingStarted();

exit:
    return res;
}

ret_code_t SampleBLEService::ConfigureGATTService(void)
{
    ret_code_t res;
    ble_add_char_params_t addCharParams;
    ble_gatts_value_t value;
    uint8_t zero = 0;

    // Initialize the nRF5 GATT module and set the allowable GATT MTU and GAP packet sizes
    // based on compile-time config values.
    res = nrf_ble_gatt_init(&sGATTModule, NULL);
    SuccessOrExit(res);
    res = nrf_ble_gatt_att_mtu_periph_set(&sGATTModule, NRF_SDH_BLE_GATT_MAX_MTU_SIZE);
    SuccessOrExit(res);
    res = nrf_ble_gatt_data_length_set(&sGATTModule, BLE_CONN_HANDLE_INVALID, NRF_SDH_BLE_GAP_DATA_LENGTH);
    SuccessOrExit(res);

    // Register vendor-specific UUIDs
    //     NOTE: An NRF_ERROR_NO_MEM here means the soft device hasn't been configured
    //     with space for enough custom UUIDs.  Typically, this limit is set by overriding
    //     the NRF_SDH_BLE_VS_UUID_COUNT config option.
    res = RegisterVendorUUID(sLEDButtonServiceUUID, sLEDButtonServiceUUID128);
    SuccessOrExit(res);
    res = RegisterVendorUUID(sButtonCharUUID, sButtonCharUUID128);
    SuccessOrExit(res);
    res = RegisterVendorUUID(sLEDCharUUID, sLEDCharUUID128);
    SuccessOrExit(res);
    res = RegisterVendorUUID(sBLEPKAPServiceUUID, sBLEPKAPServiceUUID128);
    SuccessOrExit(res);
    res = RegisterVendorUUID(sBLEPKAPAuthCharUUID, sBLEPKAPAuthCharUUID128);
    SuccessOrExit(res);

    // Add LED-BUTTON service
    res = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &sLEDButtonServiceUUID, &sLEDButtonServiceHandle);
    SuccessOrExit(res);

    // Add button characteristic
    memset(&addCharParams, 0, sizeof(addCharParams));
    addCharParams.uuid = sButtonCharUUID.uuid;
    addCharParams.uuid_type = sButtonCharUUID.type;
    addCharParams.init_len = 1;
    addCharParams.max_len = 1;
    addCharParams.char_props.read = 1;
    addCharParams.char_props.notify = 1;
    addCharParams.read_access = SEC_OPEN;
    addCharParams.cccd_write_access = SEC_OPEN;
    res = characteristic_add(sLEDButtonServiceHandle, &addCharParams, &sButtonCharHandles);
    SuccessOrExit(res);

    // Add LED characteristic
    memset(&addCharParams, 0, sizeof(addCharParams));
    addCharParams.uuid = sLEDCharUUID.uuid;
    addCharParams.uuid_type = sLEDCharUUID.type;
    addCharParams.init_len = 1;
    addCharParams.max_len = 1;
    addCharParams.char_props.read = 1;
    addCharParams.char_props.write = 1;
    addCharParams.read_access = SEC_OPEN;
    addCharParams.write_access = SEC_OPEN;
    res = characteristic_add(sLEDButtonServiceHandle, &addCharParams, &sLEDCharHandles);
    SuccessOrExit(res);

    // Add BLE-PKAP service
    res = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &sBLEPKAPServiceUUID, &sBLEPKAPServiceHandle);
    SuccessOrExit(res);

    // Add BLE-PKAP Auth characteristic
    memset(&addCharParams, 0, sizeof(addCharParams));
    addCharParams.uuid = sBLEPKAPAuthCharUUID.uuid;
    addCharParams.uuid_type = sBLEPKAPAuthCharUUID.type;
    addCharParams.init_len = 0;
    addCharParams.max_len = 32;
    addCharParams.is_var_len = 1;
    addCharParams.char_props.read = 1;
    addCharParams.char_props.write = 1;
    addCharParams.read_access = SEC_OPEN;
    addCharParams.write_access = SEC_OPEN;
    res = characteristic_add(sBLEPKAPServiceHandle, &addCharParams, &sBLEPKAPAuthCharHandles);
    SuccessOrExit(res);

    // Set the initial values of the characteristics
    memset(&value, 0, sizeof(value));
    value.len = 1;
    value.p_value = &zero;
    res = sd_ble_gatts_value_set(BLE_CONN_HANDLE_INVALID, sButtonCharHandles.value_handle, &value);
    SuccessOrExit(res);
    res = sd_ble_gatts_value_set(BLE_CONN_HANDLE_INVALID, sLEDCharHandles.value_handle, &value);
    SuccessOrExit(res);
    value.len = 0;
    res = sd_ble_gatts_value_set(BLE_CONN_HANDLE_INVALID, sBLEPKAPAuthCharHandles.value_handle, &value);
    SuccessOrExit(res);

exit:
    return res;
}

void SampleBLEService::HandleBLEEvent(ble_evt_t const * bleEvent, void * context)
{
    ret_code_t res;
    uint16_t conHandle = bleEvent->evt.gap_evt.conn_handle;

    switch (bleEvent->header.evt_id)
    {
    case BLE_GAP_EVT_CONNECTED:

        NRF_LOG_INFO("BLE connection established (con %" PRIu16 ")", conHandle);

        OnConnectionEstablished();

        // Re-enable advertising if more than one peripheral connection allowed and not at the maximum
        // number of peripheral connections.
#if NRF_SDH_BLE_PERIPHERAL_LINK_COUNT > 1
        if (ble_conn_state_peripheral_conn_count() < NRF_SDH_BLE_PERIPHERAL_LINK_COUNT)
        {
            StartAdvertising();
        }
#endif

        break;

    case BLE_GAP_EVT_DISCONNECTED:

        NRF_LOG_INFO("BLE connection terminated (con %" PRIu16 ", reason 0x%02" PRIx8 ")", conHandle, bleEvent->evt.gap_evt.params.disconnected.reason);

        {
            ble_gatts_value_t value;

            NRF_LOG_INFO("BLE-PKAP: Clearing auth state");

            // Clear the local copy of the peer auth data.
            sPeerAuthData.Clear();

            // Clear the cached Auth characteristic value.
            memset(&value, 0, sizeof(value));
            value.len = 0;
            value.p_value = (uint8_t *)&value;
            sd_ble_gatts_value_set(conHandle, sBLEPKAPAuthCharHandles.value_handle, &value);
        }

        OnConnectionTerminated();

        // Re-enable advertising if not at the maximum number of peripheral connections.
        if (ble_conn_state_peripheral_conn_count() < NRF_SDH_BLE_PERIPHERAL_LINK_COUNT)
        {
            StartAdvertising();
        }

        break;

    case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
    {
        const ble_gap_evt_sec_params_request_t * secParamsReq = &bleEvent->evt.gap_evt.params.sec_params_request;

        NRF_LOG_INFO("BLE_GAP_EVT_SEC_PARAMS_REQUEST received (con %" PRIu16 ")", conHandle);
        NRF_LOG_INFO("    bond: %" PRIu8, secParamsReq->peer_params.bond);
        NRF_LOG_INFO("    mitm: %" PRIu8, secParamsReq->peer_params.mitm);
        NRF_LOG_INFO("    lesc: %" PRIu8, secParamsReq->peer_params.lesc);
        NRF_LOG_INFO("    keypress: %" PRIu8, secParamsReq->peer_params.keypress);
        NRF_LOG_INFO("    io_caps: 0x%02" PRIX8, secParamsReq->peer_params.io_caps);
        NRF_LOG_INFO("    oob: %" PRIu8, secParamsReq->peer_params.oob);
        NRF_LOG_INFO("    min_key_size: %" PRIu8, secParamsReq->peer_params.min_key_size);
        NRF_LOG_INFO("    max_key_size: %" PRIu8, secParamsReq->peer_params.max_key_size);

        ble_gap_sec_params_t secParams;
        memset(&secParams, 0, sizeof(secParams));
        secParams.bond = 0;
        secParams.mitm = 0;
        secParams.lesc = 1;
        secParams.keypress = 0;
        secParams.io_caps = BLE_GAP_IO_CAPS_NONE;
        secParams.min_key_size = 16;
        secParams.max_key_size = 16;

        memset(&sKeySet, 0, sizeof(sKeySet));
        sKeySet.keys_own.p_pk = nrf_ble_lesc_public_key_get();
        sKeySet.keys_peer.p_pk = &sPeerLESCPubKey;
        memset(&sPeerLESCPubKey, 0, sizeof(sPeerLESCPubKey));

        uint8_t status = BLE_GAP_SEC_STATUS_SUCCESS;

        if (!secParamsReq->peer_params.lesc)
        {
            status = BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP;
            NRF_LOG_INFO("Rejecting legacy pairing");
        }

        else
        {
            if (sPeerAuthData.conHandle == conHandle)
            {
                secParams.oob = 1;
                NRF_LOG_INFO("BLE-PKAP: Initiating authenticated pairing");
            }
        }

        res = sd_ble_gap_sec_params_reply(conHandle,
                                          status,
                                          &secParams,
                                          &sKeySet);
        NRF_LOG_INFO("sd_ble_gap_sec_params_reply() result (con %" PRIu16 "): 0x%08" PRIX32, conHandle, res);
        SuccessOrExit(res);
        break;
    }

    case BLE_GAP_EVT_LESC_DHKEY_REQUEST:
    {
        char buf[kP256PubKeyCoordLength * 2 + 1];
        const ble_gap_evt_lesc_dhkey_request_t * lescDHKeyReq = &bleEvent->evt.gap_evt.params.lesc_dhkey_request;

        NRF_LOG_INFO("BLE_GAP_EVT_LESC_DHKEY_REQUEST received (con %" PRIu16 ")", conHandle);
        NRF_LOG_INFO("    Peer LESC public key:");
        ToHexString(lescDHKeyReq->p_pk_peer->pk, kP256PubKeyCoordLength, buf, sizeof(buf));
        NRF_LOG_INFO("        X: %s", buf);
        ToHexString(lescDHKeyReq->p_pk_peer->pk + kP256PubKeyCoordLength, kP256PubKeyCoordLength, buf, sizeof(buf));
        NRF_LOG_INFO("        Y: %s", buf);
        NRF_LOG_INFO("    oobd_req: %" PRIu8, lescDHKeyReq->oobd_req);

        break;
    }

    case BLE_GAP_EVT_AUTH_KEY_REQUEST:
    {
        const ble_gap_evt_auth_key_request_t * authKeyReq = &bleEvent->evt.gap_evt.params.auth_key_request;

        NRF_LOG_INFO("BLE_GAP_EVT_AUTH_KEY_REQUEST received (con %" PRIu16 ")", conHandle);
        NRF_LOG_INFO("    key_type: %" PRIu8, authKeyReq->key_type);

        // NOTE: Since this event is used for legacy pairing, and legacy pairing is unsupported,
        // we respond with a key type of NONE.
        res = sd_ble_gap_auth_key_reply(conHandle, BLE_GAP_AUTH_KEY_TYPE_NONE, NULL);
        NRF_LOG_INFO("sd_ble_gap_auth_key_reply() result (con %" PRIu16 "): 0x%08" PRIX32, conHandle, res);
        SuccessOrExit(res);
        break;
    }

    case BLE_GAP_EVT_SEC_INFO_REQUEST:
    {
        NRF_LOG_INFO("BLE_GAP_EVT_SEC_INFO_REQUEST received (con %" PRIu16 ")", conHandle);
        break;
    }

    case BLE_GAP_EVT_CONN_SEC_UPDATE:
    {
        const ble_gap_evt_conn_sec_update_t * connSecUpdate = &bleEvent->evt.gap_evt.params.conn_sec_update;

        NRF_LOG_INFO("BLE_GAP_EVT_CONN_SEC_UPDATE received (con %" PRIu16 ")", conHandle);
        NRF_LOG_INFO("    sec mode: 0x%02" PRIX8, connSecUpdate->conn_sec.sec_mode.sm);
        NRF_LOG_INFO("    sec level: 0x%02" PRIX8, connSecUpdate->conn_sec.sec_mode.lv);
        NRF_LOG_INFO("    encr_key_size: %" PRId8, connSecUpdate->conn_sec.encr_key_size);

        break;
    }

    case BLE_GAP_EVT_AUTH_STATUS:
    {
        const ble_gap_evt_auth_status_t * authStatus = &bleEvent->evt.gap_evt.params.auth_status;

        NRF_LOG_INFO("BLE_GAP_EVT_AUTH_STATUS received (con %" PRIu16 ")", conHandle);
        NRF_LOG_INFO("    auth_status: 0x%02" PRIX8, authStatus->auth_status);
        NRF_LOG_INFO("    error_src: 0x%02" PRIX8, authStatus->error_src);
        NRF_LOG_INFO("    bonded: %" PRId8, authStatus->bonded);
        NRF_LOG_INFO("    lesc: %" PRId8, authStatus->lesc);
        NRF_LOG_INFO("    sec mode 1, level 1: %" PRId8, authStatus->sm1_levels.lv1);
        NRF_LOG_INFO("    sec mode 1, level 2: %" PRId8, authStatus->sm1_levels.lv2);
        NRF_LOG_INFO("    sec mode 1, level 3: %" PRId8, authStatus->sm1_levels.lv3);
        NRF_LOG_INFO("    sec mode 1, level 4: %" PRId8, authStatus->sm1_levels.lv4);
        NRF_LOG_INFO("    sec mode 2, level 1: %" PRId8, authStatus->sm2_levels.lv1);
        NRF_LOG_INFO("    sec mode 2, level 2: %" PRId8, authStatus->sm2_levels.lv2);

        break;
    }

    case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
    {
        NRF_LOG_INFO("BLE GAP PHY update request (con %" PRIu16 ")", conHandle);
        const ble_gap_phys_t phys = { BLE_GAP_PHY_AUTO, BLE_GAP_PHY_AUTO };
        res = sd_ble_gap_phy_update(conHandle, &phys);
        NRF_LOG_INFO("sd_ble_gap_phy_update() result (con %" PRIu16 "): 0x%08" PRIX32, conHandle, res);
        SuccessOrExit(res);
        break;
    }

    case BLE_GATTS_EVT_SYS_ATTR_MISSING:
        res = sd_ble_gatts_sys_attr_set(bleEvent->evt.gatts_evt.conn_handle, NULL, 0, 0);
        NRF_LOG_INFO("sd_ble_gatts_sys_attr_set() result (con %" PRIu16 "): 0x%08" PRIX32, conHandle, res);
        SuccessOrExit(res);
        break;

    case BLE_GATTS_EVT_TIMEOUT:
        NRF_LOG_INFO("BLE GATT Server timeout (con %" PRIu16 ")", bleEvent->evt.gatts_evt.conn_handle);
        res = sd_ble_gap_disconnect(bleEvent->evt.gatts_evt.conn_handle,
                                    BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        NRF_LOG_INFO("sd_ble_gap_disconnect() result (con %" PRIu16 "): 0x%08" PRIX32, conHandle, res);
        SuccessOrExit(res);
        break;

    case BLE_GATTS_EVT_WRITE:

        if (bleEvent->evt.gatts_evt.params.write.handle == sLEDCharHandles.value_handle &&
            bleEvent->evt.gatts_evt.params.write.len == 1)
        {
            bool setOn = (bleEvent->evt.gatts_evt.params.write.data[0] != 0);
            NRF_LOG_INFO("LED characteristic write: %s", setOn ? "ON" : "OFF");
            OnLEDWrite(setOn);
        }

        else if (bleEvent->evt.gatts_evt.params.write.handle == sBLEPKAPAuthCharHandles.value_handle)
        {
            NRF_LOG_INFO("BLE-PKAP: Auth characteristic write");

            // Decode the peer authentication data
            bool decodeSuccessful = sPeerAuthData.Decode(bleEvent->evt.gatts_evt.params.write.data,
                                                         bleEvent->evt.gatts_evt.params.write.len,
                                                         conHandle);

            if (decodeSuccessful)
            {
                char buf[BLE_GAP_SEC_KEY_LEN * 2 + 1];

                NRF_LOG_INFO("BLE-PKAP: Received initiator auth token");
                ToHexString(sPeerAuthData.confirmValue, PeerAuthData::CONFIRM_VALUE_LEN, buf, sizeof(buf));
                NRF_LOG_INFO("    Confirmation Value: %s", buf);
                ToHexString(sPeerAuthData.randomValue, PeerAuthData::RANDOM_VALUE_LEN, buf, sizeof(buf));
                NRF_LOG_INFO("    Random Value: %s", buf);
            }
            else
            {
                NRF_LOG_INFO("BLE-PKAP: Invalid initiator auth token received");
            }

            // After decoding the authentication data, immediately clear the characteristic
            // value so that it can't be read.
            {
                ble_gatts_value_t value;

                NRF_LOG_INFO("BLE-PKAP: Clearing Auth characteristic");

                uint8_t zero = 0;
                memset(&value, 0, sizeof(value));
                value.len = 0;
                value.p_value = &zero;
                res = sd_ble_gatts_value_set(conHandle, sBLEPKAPAuthCharHandles.value_handle, &value);
                SuccessOrExit(res);
            }
        }

        break;

//    case BLE_GATTS_EVT_HVC:
//        err = HandleTXComplete(event);
//        SuccessOrExit(err);
//        break;

    default:
        NRF_LOG_INFO("Other BLE event %" PRIu16 "(con %" PRIu16 ")", bleEvent->header.evt_id, conHandle);
        break;
    }

    nrf_ble_lesc_on_ble_evt(bleEvent);

exit:
    return;
}

ret_code_t SampleBLEService::RegisterVendorUUID(ble_uuid_t & uuid, const ble_uuid128_t & vendorUUID)
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
    SuccessOrExit(res);

    // Initialize the SoftDevice "short" UUID structure that will be used to refer to this vendor UUID.
    uuid.type = shortUUIDType;
    uuid.uuid = (((uint16_t)vendorUUID.uuid128[13]) << 8) | vendorUUID.uuid128[12];

exit:
    return res;
}

ble_gap_lesc_oob_data_t *SampleBLEService::GetPeerLESCOOBData(uint16_t conHandle)
{
    ret_code_t res;
    static ble_gap_lesc_oob_data_t lescOOBData;

    // If the initiator didn't write an auth token to the Auth characteristic prior to starting
    // pairing, force OOB pairing to fail by returning a NULL.
    if (sPeerAuthData.conHandle != conHandle)
    {
        NRF_LOG_INFO("BLE-PKAP: Pairing failed - No initiator auth token received");
        return NULL;
    }

    // Initialize the OOB data structure needed by the SoftDevice to confirm OOB pairing.
    //
    // TODO: reduce global memory usage by eliminating lescOOBData and unioning the
    // ble_gap_lesc_oob_data_t structure with sPeerAuthData
    //
    memset(&lescOOBData, 0, sizeof(lescOOBData));
    memcpy(lescOOBData.r, sPeerAuthData.randomValue, sizeof(lescOOBData.r));

    // Compute the expected confirmation value given the initiator's public key and the random
    // value supplied in the auth data.
    nrf5utils::ComputeLESCOOBConfirmationValue(sPeerLESCPubKey.pk, sPeerAuthData.randomValue, lescOOBData.c);

    {
        char buf[BLE_GAP_SEC_KEY_LEN * 2 + 1];
        ToHexString(lescOOBData.c, BLE_GAP_SEC_KEY_LEN, buf, sizeof(buf));
        NRF_LOG_INFO("BLE-PKAP: Expected initiator confirmation value: %s", buf);
    }

    // MOCK authentication of the peer by comparing the computed confirmation value to
    // the value supplied in the peer's auth data.  If they don't match, fail pairing.
    //
    // NOTE THAT THIS IS NOT SECURE
    //
    if (memcmp(lescOOBData.c, sPeerAuthData.confirmValue, PeerAuthData::CONFIRM_VALUE_LEN) != 0)
    {
        NRF_LOG_INFO("BLE-PKAP: Pairing failed - Initiator confirmation value mismatch");
        return NULL;
    }

    NRF_LOG_INFO("BLE-PKAP: Initiator authentication successful");

    // Set the Auth characteristic value to a MOCK of the responder's auth token (in this case,
    // just the confirmation value sent by the initiator).
    //
    // NOTE THAT THIS IS NOT SECURE
    //
    {
        ble_gatts_value_t value;
        char buf[BLE_GAP_SEC_KEY_LEN * 2 + 1];

        NRF_LOG_INFO("BLE-PKAP: Publishing responder auth token");
        ToHexString(sPeerAuthData.confirmValue, PeerAuthData::CONFIRM_VALUE_LEN, buf, sizeof(buf));
        NRF_LOG_INFO("    Confirmation Value: %s", buf);

        memset(&value, 0, sizeof(value));
        value.len = PeerAuthData::CONFIRM_VALUE_LEN;
        value.p_value = sPeerAuthData.confirmValue;
        res = sd_ble_gatts_value_set(conHandle, sBLEPKAPAuthCharHandles.value_handle, &value);
        if (res != NRF_SUCCESS)
        {
            NRF_LOG_INFO("sd_ble_gatts_value_set failed: %lu", res);
            return NULL;
        }
    }

    // Clear the stored peer auth state so that subsequent attempts at pairing don't reuse
    // stale data.
    sPeerAuthData.Clear();

    return &lescOOBData;
}

#endif // defined(SOFTDEVICE_PRESENT) && SOFTDEVICE_PRESENT
