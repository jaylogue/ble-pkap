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
#include <algorithm>

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
#include "nrf_crypto.h"
#include "nrf_crypto_error.h"

#if NRF_LOG_ENABLED
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#endif // NRF_LOG_ENABLED

#include <BLEPKAP.h>
#include <BLEPKAPService.h>
#include <FunctExitUtils.h>
#include <nRF5LESCOOB.h>

namespace {

constexpr size_t kP256PubKeyCoordLength = 32;

const ble_uuid128_t kLEDButtonServiceUUID128 = { { 0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15, 0xDE, 0xEF, 0x12, 0x12, 0x23, 0x15, 0x00, 0x00 } };
const ble_uuid128_t kButtonCharUUID128       = { { 0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15, 0xDE, 0xEF, 0x12, 0x12, 0x24, 0x15, 0x00, 0x00 } };
const ble_uuid128_t kLEDCharUUID128          = { { 0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15, 0xDE, 0xEF, 0x12, 0x12, 0x25, 0x15, 0x00, 0x00 } };

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

// Local device's BLE-PKAP private key
// 
// This is used by the local device to prove its identity to the peer during the BLE-PKAP
// pairing process.  In this example, the private key is hardcoded into the firmware image.
// In a production setting, each device would have a unique private key programmed into it
// as part of a manufacturing or device enrollment process.
//
const uint8_t kDevicePrivKey[] =
{ 
    // -----BEGIN EC PRIVATE KEY-----
    // MHcCAQEEIO6Wr7oIFjaQPG6YLtBMWwwJEt7YRHmcvlgvKxoPFudzoAoGCCqGSM49
    // AwEHoUQDQgAEgSLr4fEu5N6NytlnJ+mbOCb+hU/vWwUkQpBWymjRocbyMTBokbam
    // QpO8zDEHAvLeReWj27w6WAoUhSNZV5Q1nA==
    // -----END EC PRIVATE KEY-----

    0xee, 0x96, 0xaf, 0xba, 0x08, 0x16, 0x36, 0x90, 0x3c, 0x6e, 0x98, 0x2e, 0xd0, 0x4c, 0x5b, 0x0c, 
    0x09, 0x12, 0xde, 0xd8, 0x44, 0x79, 0x9c, 0xbe, 0x58, 0x2f, 0x2b, 0x1a, 0x0f, 0x16, 0xe7, 0x73
};
constexpr uint16_t kDeviceKeyId = 1;

// Trusted peer's BLE-PKAP public key
//
// This is the public key of a peer node that this device trusts.  It is used by the 
// local device to authenticate the peer during the BLE-PKAP pairing process.  In this
// example, a single trusted public key is hardcoded into the firmware image.  In a
// production setting, the trusted public key (or keys) would be programmed into the
// device during manufacturing or device enrollment.
//
const uint8_t kTrustedPeerPubKey[] =
{
    // -----BEGIN PUBLIC KEY-----
    // MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEgSLr4fEu5N6NytlnJ+mbOCb+hU/v
    // WwUkQpBWymjRocbyMTBokbamQpO8zDEHAvLeReWj27w6WAoUhSNZV5Q1nA==
    // -----END PUBLIC KEY-----

    // X
    0x81, 0x22, 0xeb, 0xe1, 0xf1, 0x2e, 0xe4, 0xde, 0x8d, 0xca, 0xd9, 0x67, 0x27, 0xe9, 0x9b, 0x38,
    0x26, 0xfe, 0x85, 0x4f, 0xef, 0x5b, 0x05, 0x24, 0x42, 0x90, 0x56, 0xca, 0x68, 0xd1, 0xa1, 0xc6, 
    // Y
    0xf2, 0x31, 0x30, 0x68, 0x91, 0xb6, 0xa6, 0x42, 0x93, 0xbc, 0xcc, 0x31, 0x07, 0x02, 0xf2, 0xde,
    0x45, 0xe5, 0xa3, 0xdb, 0xbc, 0x3a, 0x58, 0x0a, 0x14, 0x85, 0x23, 0x59, 0x57, 0x94, 0x35, 0x9c
};
constexpr uint16_t kTrustedPeerKeyId = 1;

// BLE-PKAP authentication state
struct BLEPKAPAuthState
{
    enum {
        kState_Idle = 0,
        kState_TokenReceived = 1,
        kState_OOBPairingStarted = 2,
        kState_PeerLESCPubKeyReceived = 3,
        kState_PeerAuthenticated = 4,
        kState_PairingComplete = 5
    };

    static constexpr size_t kMaxAuthTokenLen = std::max(BLEPKAP::InitiatorAuthToken::kTokenLen,
                                                        BLEPKAP::ResponderAuthToken::kTokenLen);

    ble_gap_lesc_p256_pk_t PeerLESCPubKey;
    uint8_t AuthTokenBuf[kMaxAuthTokenLen];
    uint16_t AuthConHandle;
    uint8_t State;

    void Clear() { *this = {}; }
} sBLEPKAPAuthState;

void ToHexString(const uint8_t * data, size_t dataLen, char * outBuf, size_t outBufSize)
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


ret_code_t BLEPKAPService::Init(void)
{
    ret_code_t res;

    sBLEPKAPAuthState.Clear();

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, BLE_OBSERVER_PRIO, BLEPKAPService::HandleBLEEvent, NULL);

    ble_conn_state_init();

    res = nrf_ble_lesc_init();
    SuccessOrExit(res);

    // Register callback for when the SoftDevice requests the peer's LESC OOB data.
    nrf_ble_lesc_peer_oob_data_handler_set(GetPeerLESCOOBData);

    res = SetDeviceName();
    SuccessOrExit(res);

    res = ConfigureGATTService();
    SuccessOrExit(res);

    res = ConfigureAdvertising();
    SuccessOrExit(res);

    res = StartAdvertising();
    SuccessOrExit(res);

exit:
    return res;
}

void BLEPKAPService::Shutdown(void)
{

}

void BLEPKAPService::UpdateButtonState(bool isPressed)
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

ret_code_t BLEPKAPService::SetDeviceName(void)
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

ret_code_t BLEPKAPService::ConfigureAdvertising(void)
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

ret_code_t BLEPKAPService::StartAdvertising(void)
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

ret_code_t BLEPKAPService::ConfigureGATTService(void)
{
    ret_code_t res;
    ble_gatts_attr_t attr;
    ble_gatts_attr_md_t attrMD, cccdAttrMD;
    ble_gatts_char_md_t charMD;
    uint8_t zero = 0;

    NRF_LOG_INFO("Configuring BLE GATT service");

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
    res = RegisterVendorUUID(sLEDButtonServiceUUID, kLEDButtonServiceUUID128);
    SuccessOrExit(res);
    res = RegisterVendorUUID(sButtonCharUUID, kButtonCharUUID128);
    SuccessOrExit(res);
    res = RegisterVendorUUID(sLEDCharUUID, kLEDCharUUID128);
    SuccessOrExit(res);
    res = RegisterVendorUUID(sBLEPKAPServiceUUID, BLEPKAP::kBLEPKAPServiceUUID128);
    SuccessOrExit(res);
    res = RegisterVendorUUID(sBLEPKAPAuthCharUUID, BLEPKAP::kBLEPKAPAuthCharUUID128);
    SuccessOrExit(res);

    NRF_LOG_INFO("Adding LED-BUTTON service");

    // Add LED-BUTTON service
    res = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &sLEDButtonServiceUUID, &sLEDButtonServiceHandle);
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
    BLE_GAP_CONN_SEC_MODE_SET_LESC_ENC_WITH_MITM(&attrMD.read_perm);
    memset(&charMD, 0, sizeof(charMD));
    charMD.char_props.read = 1;
    charMD.char_props.notify = 1;
    charMD.p_cccd_md  = &cccdAttrMD;
    memset(&cccdAttrMD, 0, sizeof(cccdAttrMD));
    cccdAttrMD.vloc = BLE_GATTS_VLOC_STACK;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccdAttrMD.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_LESC_ENC_WITH_MITM(&cccdAttrMD.write_perm);
    res = sd_ble_gatts_characteristic_add(sLEDButtonServiceHandle, &charMD, &attr, &sButtonCharHandles);
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
    BLE_GAP_CONN_SEC_MODE_SET_LESC_ENC_WITH_MITM(&attrMD.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_LESC_ENC_WITH_MITM(&attrMD.write_perm);
    memset(&charMD, 0, sizeof(charMD));
    charMD.char_props.read = 1;
    charMD.char_props.write = 1;
    res = sd_ble_gatts_characteristic_add(sLEDButtonServiceHandle, &charMD, &attr, &sLEDCharHandles);
    SuccessOrExit(res);

    NRF_LOG_INFO("Adding BLE-PKAP service");

    // Add BLE-PKAP service
    res = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &sBLEPKAPServiceUUID, &sBLEPKAPServiceHandle);
    SuccessOrExit(res);

    // Add BLE-PKAP Auth characteristic
    memset(&attr, 0, sizeof(attr));
    attr.p_uuid = &sBLEPKAPAuthCharUUID;
    attr.p_attr_md = &attrMD;
    attr.max_len = 128;
    attr.init_len = 0;
    attr.p_value  = &zero;
    memset(&attrMD, 0, sizeof(attrMD));
    attrMD.vloc = BLE_GATTS_VLOC_STACK;
    attrMD.vlen = 1;
    BLE_GAP_CONN_SEC_MODE_SET_LESC_ENC_WITH_MITM(&attrMD.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attrMD.write_perm);
    memset(&charMD, 0, sizeof(charMD));
    charMD.char_props.read = 1;
    charMD.char_props.write = 1;
    res = sd_ble_gatts_characteristic_add(sBLEPKAPServiceHandle, &charMD, &attr, &sBLEPKAPAuthCharHandles);
    SuccessOrExit(res);

exit:
    if (res != NRF_SUCCESS)
        NRF_LOG_INFO("ConfigureGATTService() failed: %" PRId32, res);
    return res;
}

void BLEPKAPService::HandleBLEEvent(ble_evt_t const * bleEvent, void * context)
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

        // If BLE-PKAP pairing was performed on this connection...
        if (sBLEPKAPAuthState.State != BLEPKAPAuthState::kState_Idle && sBLEPKAPAuthState.AuthConHandle == conHandle)
        {
            ClearAuthState();

            // Force regeneration of local LESC public/private keys.
            // This is necessary (although by itself not sufficient) to ensure the untrackability
            // of the device.
            nrf_ble_lesc_keypair_generate();
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

        uint8_t secStatus;

        ble_gap_sec_params_t secParams;
        memset(&secParams, 0, sizeof(secParams));

        ble_gap_sec_keyset_t keySet;
        memset(&keySet, 0, sizeof(keySet));

        // If the peer has requested BLE-PKAP pairing...
        if (sBLEPKAPAuthState.State == BLEPKAPAuthState::kState_TokenReceived &&
            sBLEPKAPAuthState.AuthConHandle == conHandle &&
            secParamsReq->peer_params.lesc)
        {
            NRF_LOG_INFO("BLE-PKAP: Starting LESC OOB pairing");
            sBLEPKAPAuthState.State = BLEPKAPAuthState::kState_OOBPairingStarted;

            // Instruct the SoftDevice perform LESC OOB pairing of the peer.
            secStatus = BLE_GAP_SEC_STATUS_SUCCESS;
            secParams.oob = 1;
            secParams.lesc = 1;
            secParams.min_key_size = 16;
            secParams.max_key_size = 16;

            // Supply the local LESC public key to the SoftDevice and provide space to 
            // receive the peer's public key.
            keySet.keys_own.p_pk = nrf_ble_lesc_public_key_get();
            keySet.keys_peer.p_pk = &sBLEPKAPAuthState.PeerLESCPubKey;

            {
                char buf[kP256PubKeyCoordLength * 2 + 1];

                NRF_LOG_INFO("    Local LESC public key:");
                ToHexString(keySet.keys_own.p_pk->pk, kP256PubKeyCoordLength, buf, sizeof(buf));
                NRF_LOG_INFO("        X: (%" PRId32 ") %s", kP256PubKeyCoordLength, buf);
                ToHexString(keySet.keys_own.p_pk->pk + kP256PubKeyCoordLength, kP256PubKeyCoordLength, buf, sizeof(buf));
                NRF_LOG_INFO("        Y: (%" PRId32 ") %s", kP256PubKeyCoordLength, buf);
            }
        }

        // otherwise, reject the pairing attempt.
        else
        {
            secStatus = BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP;
            NRF_LOG_INFO("Rejecting non-BLE-PKAP pairing request");
        }

        res = sd_ble_gap_sec_params_reply(conHandle, secStatus, &secParams, &keySet);
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
        NRF_LOG_INFO("        X: (%" PRId32 ") %s", kP256PubKeyCoordLength, buf);
        ToHexString(lescDHKeyReq->p_pk_peer->pk + kP256PubKeyCoordLength, kP256PubKeyCoordLength, buf, sizeof(buf));
        NRF_LOG_INFO("        Y: (%" PRId32 ") %s", kP256PubKeyCoordLength, buf);
        NRF_LOG_INFO("    oobd_req: %" PRIu8, lescDHKeyReq->oobd_req);

        if (sBLEPKAPAuthState.State == BLEPKAPAuthState::kState_OOBPairingStarted &&
            sBLEPKAPAuthState.AuthConHandle == conHandle)
        {
            sBLEPKAPAuthState.State = BLEPKAPAuthState::kState_PeerLESCPubKeyReceived;
            NRF_LOG_INFO("BLE-PKAP: Peer LESC public key received");
        }

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

        if (sBLEPKAPAuthState.State == BLEPKAPAuthState::kState_PeerAuthenticated &&
            sBLEPKAPAuthState.AuthConHandle == conHandle)
        {
            if (authStatus->auth_status != BLE_GAP_SEC_STATUS_SUCCESS)
            {
                NRF_LOG_INFO("BLE-PKAP: LESC OOB pairing FAILED");
                ClearAuthState();
            }

            else if (!authStatus->lesc || !authStatus->sm1_levels.lv4)
            {
                NRF_LOG_INFO("BLE-PKAP: Unexpected authentication state");
                ClearAuthState();
            }

            else
            {
                sBLEPKAPAuthState.State = BLEPKAPAuthState::kState_PairingComplete;
                NRF_LOG_INFO("BLE-PKAP: Pairing complete");
            }
        }

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
            BLEPKAP::InitiatorAuthToken authToken;

            NRF_LOG_INFO("BLE-PKAP: Auth characteristic write");

            // If no BLE-PKAP pairing is in progress...
            if (sBLEPKAPAuthState.State == BLEPKAPAuthState::kState_Idle)
            {
                // Verify the size and structure of the peers's auth token by
                // attempting to decode it.
                res = authToken.Decode(bleEvent->evt.gatts_evt.params.write.data,
                                       bleEvent->evt.gatts_evt.params.write.len);

                if (res != NRF_SUCCESS)
                {
                    NRF_LOG_INFO("BLE-PKAP: Invalid auth token received from peer: %" PRId32, res);
                }

                // If successful, verify the peer is using a known key.
                if (res == NRF_SUCCESS && authToken.KeyId != kTrustedPeerKeyId)
                {
                    NRF_LOG_INFO("BLE-PKAP: Unknown peer key id: %" PRIu16, authToken.KeyId);
                    res = NRF_ERROR_NOT_FOUND;
                }

                if (res == NRF_SUCCESS)
                {
                    char buf[BLEPKAP::InitiatorAuthToken::kSigLen + 1];

                    NRF_LOG_INFO("BLE-PKAP: Received peer auth token (len %" PRIu16 ")", bleEvent->evt.gatts_evt.params.write.len);
                    NRF_LOG_INFO("    Format: %" PRIu8, authToken.Format);
                    NRF_LOG_INFO("    KeyId: %" PRIu16, authToken.KeyId);
                    ToHexString(authToken.Sig, BLEPKAP::InitiatorAuthToken::kSigLen, buf, sizeof(buf));
                    NRF_LOG_INFO("    Sig: (%" PRId32 ") %s", BLEPKAP::InitiatorAuthToken::kSigLen, buf);
                    ToHexString(authToken.Random, BLEPKAP::InitiatorAuthToken::kRandomLen, buf, sizeof(buf));
                    NRF_LOG_INFO("    Random: (%" PRId32 ") %s", BLEPKAP::InitiatorAuthToken::kRandomLen, buf);

                    // Save the auth token for later use
                    memcpy(sBLEPKAPAuthState.AuthTokenBuf, bleEvent->evt.gatts_evt.params.write.data, BLEPKAP::InitiatorAuthToken::kTokenLen);

                    // Begin the BLE-PKAP protocol.
                    sBLEPKAPAuthState.State = BLEPKAPAuthState::kState_TokenReceived;
                    sBLEPKAPAuthState.AuthConHandle = conHandle;
                    NRF_LOG_INFO("BLE-PKAP: Starting BLE-PKAP protocol");
                }
            }

            else
            {
                NRF_LOG_INFO("BLE-PKAP: Pairing already in progress - Ignoring auth characteristic write");
            }

            // Immediately clear the auth characteristic value so that it can't be read.
            {
                ble_gatts_value_t value;
                memset(&value, 0, sizeof(value));
                value.len = 0;
                value.p_value = (uint8_t *)&value;
                res = sd_ble_gatts_value_set(BLE_GAP_ADV_SET_HANDLE_NOT_SET, sBLEPKAPAuthCharHandles.value_handle, &value);
                SuccessOrExit(res);
                NRF_LOG_INFO("BLE-PKAP: Auth characteristic cleared");
            }
        }

        break;

    default:
        NRF_LOG_INFO("Other BLE event %" PRIu16 "(con %" PRIu16 ")", bleEvent->header.evt_id, conHandle);
        break;
    }

    nrf_ble_lesc_on_ble_evt(bleEvent);

exit:
    return;
}

ret_code_t BLEPKAPService::RegisterVendorUUID(ble_uuid_t & uuid, const ble_uuid128_t & vendorUUID)
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

ble_gap_lesc_oob_data_t *BLEPKAPService::GetPeerLESCOOBData(uint16_t conHandle)
{
    ret_code_t res;
    static ble_gap_lesc_oob_data_t lescOOBData;

    //
    // This function is called by the nrf_ble_lesc module during LESC OOB pairing,
    // at the point the SoftDevice needs the peer's OOB data.  This is where the
    // bulk of the BLE-PKAP authentication process happens.
    //

    // Verify we're in the correct state.  The code in HandleBLEEvent() that
    // handles the BLE_GAP_EVT_SEC_PARAMS_REQUEST event should ensure that
    // this is always the case, but we double check here.
    if (sBLEPKAPAuthState.State != BLEPKAPAuthState::kState_PeerLESCPubKeyReceived || 
        sBLEPKAPAuthState.AuthConHandle != conHandle)
    {
        NRF_LOG_INFO("BLE-PKAP: Pairing failed - Unexpected state in GetPeerLESCOOBData()");
        ExitNow(res = NRF_ERROR_INVALID_STATE);
    }

    // Authenticate the peer using their auth token...

    {
        BLEPKAP::InitiatorAuthToken initAuthToken;

        // Decode the peer's auth token.
        res = initAuthToken.Decode(sBLEPKAPAuthState.AuthTokenBuf, BLEPKAP::InitiatorAuthToken::kTokenLen);
        if (res != NRF_SUCCESS)
        {
            NRF_LOG_INFO("InitiatorAuthToken::Decode() failed: %" PRIu32, res);
            ExitNow();
        }

        // Initialize the OOB data structure needed by the SoftDevice to confirm OOB pairing.
        //
        // TODO: reduce global memory usage by eliminating lescOOBData and unioning the
        // ble_gap_lesc_oob_data_t structure with sBLEPKAPAuthState??? 
        //
        memset(&lescOOBData, 0, sizeof(lescOOBData));
        memcpy(lescOOBData.r, initAuthToken.Random, sizeof(lescOOBData.r));

        // Compute the expected OOB confirmation value for the peer given the peer's LESC public
        // key and the random value supplied in the auth token.
        nrf5utils::ComputeLESCOOBConfirmationValue(sBLEPKAPAuthState.PeerLESCPubKey.pk, initAuthToken.Random, lescOOBData.c);

        {
            char buf[BLE_GAP_SEC_KEY_LEN * 2 + 1];
            ToHexString(lescOOBData.c, BLE_GAP_SEC_KEY_LEN, buf, sizeof(buf));
            NRF_LOG_INFO("BLE-PKAP: Expected peer OOB confirmation value: (%" PRId32 ") %s", BLE_GAP_SEC_KEY_LEN, buf);
        }

        // Verify the signature of the confirmation value contained in the auth token against
        // the peer's public key.
        res = initAuthToken.Verify(lescOOBData.c, sizeof(lescOOBData.c), kTrustedPeerPubKey, sizeof(kTrustedPeerPubKey));
        if (res != NRF_SUCCESS)
        {
            if (res == NRF_ERROR_CRYPTO_ECDSA_INVALID_SIGNATURE)
            {
                NRF_LOG_INFO("BLE-PKAP: Pairing failed - Peers's auth token failed verification");
            }
            else
            {
                NRF_LOG_INFO("BLE-PKAP: Pairing failed - Signature verification error: %" PRIu32, res);
            }
            ExitNow();
        }

        sBLEPKAPAuthState.State = BLEPKAPAuthState::kState_PeerAuthenticated;
        NRF_LOG_INFO("BLE-PKAP: Peer authentication SUCCESSFUL");
    }

    // Generate and publish the local device's auth token so that the peer can authenticate us...

    {
        ble_gatts_value_t value;
        size_t respAuthTokenLen;

        // Generate a responder auth token based on the initiator's OOB confirmation value and
        // the local device's private authentication key.
        respAuthTokenLen = sizeof(sBLEPKAPAuthState.AuthTokenBuf);
        res = BLEPKAP::ResponderAuthToken::Generate(kDeviceKeyId, lescOOBData.c, sizeof(lescOOBData.c),
                                                    kDevicePrivKey, sizeof(kDevicePrivKey),
                                                    sBLEPKAPAuthState.AuthTokenBuf, respAuthTokenLen);
        if (res != NRF_SUCCESS)
        {
            NRF_LOG_INFO("ResponderAuthToken::Generate() failed: %" PRIu32, res);
            ExitNow();
        }

        {
            BLEPKAP::ResponderAuthToken authToken;
            char buf[BLEPKAP::ResponderAuthToken::kSigLen + 1];

            authToken.Decode(sBLEPKAPAuthState.AuthTokenBuf, respAuthTokenLen);
            NRF_LOG_INFO("BLE-PKAP: Generated responder auth token (len %" PRIu32 ")", respAuthTokenLen);
            NRF_LOG_INFO("    Format: %" PRIu8, authToken.Format);
            NRF_LOG_INFO("    KeyId: %" PRIu16, authToken.KeyId);
            ToHexString(authToken.Sig, BLEPKAP::ResponderAuthToken::kSigLen, buf, sizeof(buf));
            NRF_LOG_INFO("    Sig: (%" PRId32 ") %s", BLEPKAP::ResponderAuthToken::kSigLen, buf);
        }

        // Publish the responder auth token as the value of the auth characteristic, such that
        // the peer may read it and use it to authenticate the local device.
        memset(&value, 0, sizeof(value));
        value.len = respAuthTokenLen;
        value.p_value = sBLEPKAPAuthState.AuthTokenBuf;
        res = sd_ble_gatts_value_set(conHandle, sBLEPKAPAuthCharHandles.value_handle, &value);
        if (res != NRF_SUCCESS)
        {
            NRF_LOG_INFO("sd_ble_gatts_value_set failed: %" PRIu32, res);
            ExitNow();
        }
    }

exit:

    if (res != NRF_SUCCESS)
    {
        ClearAuthState();

        // In the case of an error, return invalid LESC OOB data, which will cause
        // the BLE OOB pairing process to fail and result in the peer receiving an
        // authentication error.  This is preferable to returning NULL which will
        // trigger an internal error in the nrf_ble_lesc module that can only be
        // reset by re-initializing the module.
        memset(&lescOOBData, 42, sizeof(lescOOBData));
    }

    return &lescOOBData;
}

void BLEPKAPService::ClearAuthState(void)
{
    // Clear the local BLE-PKAP authentication state.
    sBLEPKAPAuthState.Clear();

    // Clear the cached Auth characteristic value.
    {
        ble_gatts_value_t value;
        memset(&value, 0, sizeof(value));
        value.len = 0;
        value.p_value = (uint8_t *)&value;
        sd_ble_gatts_value_set(BLE_GAP_ADV_SET_HANDLE_NOT_SET, sBLEPKAPAuthCharHandles.value_handle, &value);
    }

    NRF_LOG_INFO("BLE-PKAP: Auth state cleared");
}

#endif // defined(SOFTDEVICE_PRESENT) && SOFTDEVICE_PRESENT
