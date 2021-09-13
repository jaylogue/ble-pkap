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
 *         An implementation of the BLE Public Key Authenticated Pairing (BLE-PKAP) protocol
 *         for the Nordic nRF5 SDK.
 */

#include <sdk_common.h>

#if defined(SOFTDEVICE_PRESENT) && SOFTDEVICE_PRESENT

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <algorithm>

#include <ble.h>
#include <nrf_ble_lesc.h>
#include <nrf_crypto.h>

#if NRF_LOG_ENABLED
#include <nrf_log.h>
#endif // NRF_LOG_ENABLED

#include <BLEPKAP.h>
#include <BLEPKAPService.h>
#include <FunctExitUtils.h>
#include <LESCOOB.h>
#include <nRF5Utils.h>

using namespace nrf5utils;

namespace {

const ble_uuid128_t kBLEPKAPServiceUUID128   = { { 0x24, 0x6b, 0x33, 0x15, 0x5f, 0x1c, 0x3c, 0x58, 0xc0, 0xe6, 0x2c, 0xbc, 0x00, 0xee, 0x78, 0xe2 } };
const ble_uuid128_t kBLEPKAPAuthCharUUID128  = { { 0x24, 0x6b, 0x33, 0x15, 0x5f, 0x1c, 0x3c, 0x58, 0xc0, 0xe6, 0x2c, 0xbc, 0x01, 0xee, 0x78, 0xe2 } };;

ble_uuid_t sBLEPKAPServiceUUID;
ble_uuid_t sBLEPKAPAuthCharUUID;
uint16_t sBLEPKAPServiceHandle;
ble_gatts_char_handles_t sBLEPKAPAuthCharHandles;

uint16_t sDeviceKeyId;
const uint8_t * sDevicePrivKey;
size_t sDevicePrivKeyLen;

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


ret_code_t BLEPKAPService::Init(uint16_t deviceKeyId, const uint8_t * devicePrivKey, size_t devicePrivKeyLen)
{
    ret_code_t res;
    ble_gatts_attr_t attr;
    ble_gatts_attr_md_t attrMD;
    ble_gatts_char_md_t charMD;
    uint8_t zero = 0;

    // Static declaration of BLE observer for BLEPKAPService class.
    NRF_SDH_BLE_OBSERVER(BLEObserver, BLE_PKAP_SERVICE_OBSERVER_PRIO, BLEPKAPService::HandleBLEEvent, NULL);

    // Verify and save the device's private key and key id.
    VerifyOrExit(devicePrivKeyLen == NRF_CRYPTO_ECC_SECP256K1_RAW_PRIVATE_KEY_SIZE, res = NRF_ERROR_INVALID_PARAM);
    sDeviceKeyId = deviceKeyId;
    sDevicePrivKey = devicePrivKey;
    sDevicePrivKeyLen = devicePrivKeyLen;

    NRF_LOG_INFO("Adding BLE-PKAP service");

    // Initialize the nrf_ble_lesc module.
    res = nrf_ble_lesc_init();
    NRF_LOG_CALL_FAIL_INFO("nrf_ble_lesc_init", res);
    SuccessOrExit(res);

    // Register vendor-specific UUIDs
    //     NOTE: An NRF_ERROR_NO_MEM here means the soft device hasn't been configured
    //     with space for enough custom UUIDs.  Typically, this limit is set by overriding
    //     the NRF_SDH_BLE_VS_UUID_COUNT config option.
    res = RegisterVendorUUID(sBLEPKAPServiceUUID, kBLEPKAPServiceUUID128);
    SuccessOrExit(res);
    res = RegisterVendorUUID(sBLEPKAPAuthCharUUID, kBLEPKAPAuthCharUUID128);
    SuccessOrExit(res);

    // Add BLE-PKAP service
    res = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &sBLEPKAPServiceUUID, &sBLEPKAPServiceHandle);
    NRF_LOG_CALL_FAIL_INFO("sd_ble_gatts_service_add", res);
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
    NRF_LOG_CALL_FAIL_INFO("sd_ble_gatts_characteristic_add", res);
    SuccessOrExit(res);

    // Register callback for when the SoftDevice requests the peer's LESC OOB data.
    nrf_ble_lesc_peer_oob_data_handler_set(GetPeerLESCOOBData);

    sBLEPKAPAuthState.Clear();

exit:
    return res;
}

ret_code_t BLEPKAPService::RunMainLoopActions(void)
{
    ret_code_t res = NRF_SUCCESS;

    res = nrf_ble_lesc_request_handler();
    NRF_LOG_CALL_FAIL_INFO("nrf_ble_lesc_request_handler", res);

    return res;
}

void BLEPKAPService::HandleBLEEvent(ble_evt_t const * bleEvent, void * context)
{
    ret_code_t res;
    uint16_t conHandle = bleEvent->evt.gap_evt.conn_handle;

    switch (bleEvent->header.evt_id)
    {
    case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
    {
        const ble_gap_evt_sec_params_request_t * secParamsReq = &bleEvent->evt.gap_evt.params.sec_params_request;

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
            NRF_LOG_INFO("BLE-PKAP: Rejecting non-BLE-PKAP pairing request");

            // TODO: clear auth state if associated with event connection
        }

        res = sd_ble_gap_sec_params_reply(conHandle, secStatus, &secParams, &keySet);
        NRF_LOG_CALL_FAIL_INFO("sd_ble_gap_sec_params_reply", res);
        break;
    }

    case BLE_GAP_EVT_LESC_DHKEY_REQUEST:
    {
        if (sBLEPKAPAuthState.State == BLEPKAPAuthState::kState_OOBPairingStarted &&
            sBLEPKAPAuthState.AuthConHandle == conHandle)
        {
            sBLEPKAPAuthState.State = BLEPKAPAuthState::kState_PeerLESCPubKeyReceived;
            NRF_LOG_INFO("BLE-PKAP: Peer LESC public key received");
        }

        break;
    }

    case BLE_GAP_EVT_AUTH_KEY_REQUEST:
        // Reject legacy pairing
        res = sd_ble_gap_auth_key_reply(conHandle, BLE_GAP_AUTH_KEY_TYPE_NONE, NULL);
        NRF_LOG_CALL_FAIL_INFO("sd_ble_gap_auth_key_reply", res);
        break;

    case BLE_GAP_EVT_AUTH_STATUS:
    {
        const ble_gap_evt_auth_status_t * authStatus = &bleEvent->evt.gap_evt.params.auth_status;

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

    case BLE_GATTS_EVT_WRITE:

        // If the auth characteristic is being written...
        if (bleEvent->evt.gatts_evt.params.write.handle == sBLEPKAPAuthCharHandles.value_handle)
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
                if (res == NRF_SUCCESS && !Callback::IsKnownPeerKeyId(authToken.KeyId))
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
                NRF_LOG_CALL_FAIL_INFO("sd_ble_gatts_value_set", res);
                SuccessOrExit(res);
                NRF_LOG_INFO("BLE-PKAP: Auth characteristic cleared");
            }
        }

        break;
    }

    nrf_ble_lesc_on_ble_evt(bleEvent);

exit:

    // TODO: clear auth state if error and assocaited with event connection

    return;
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
        NRF_LOG_CALL_FAIL_INFO("InitiatorAuthToken::Decode", res);
        SuccessOrExit(res);

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

        const uint8_t * peerPubKey;
        size_t peerPubKeyLen;

        // Invoke the application's callback to get the trusted public key for the peer.
        res = Callback::GetPeerPublicKey(initAuthToken.KeyId, peerPubKey, peerPubKeyLen);
        NRF_LOG_CALL_FAIL_INFO("BLEPKAPService::Callback::GetPeerPublicKey", res);
        if (res != NRF_SUCCESS)
        {
            NRF_LOG_INFO("BLE-PKAP: Pairing failed - Unable to retrieve peer's public key");
            ExitNow();
        }

        // Verify the signature of the confirmation value contained in the auth token against
        // the peer's public key.
        res = initAuthToken.Verify(lescOOBData.c, sizeof(lescOOBData.c), peerPubKey, peerPubKeyLen);
        NRF_LOG_CALL_FAIL_INFO("InitiatorAuthToken::Verify", res);
        if (res != NRF_SUCCESS)
        {
            if (res == NRF_ERROR_CRYPTO_ECDSA_INVALID_SIGNATURE)
            {
                NRF_LOG_INFO("BLE-PKAP: Pairing failed - Peers's auth token failed verification");
            }
            else
            {
                NRF_LOG_INFO("BLE-PKAP: Pairing failed - Signature verification error");
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
        res = BLEPKAP::ResponderAuthToken::Generate(sDeviceKeyId, lescOOBData.c, sizeof(lescOOBData.c),
                                                    sDevicePrivKey, sDevicePrivKeyLen,
                                                    sBLEPKAPAuthState.AuthTokenBuf, respAuthTokenLen);
        NRF_LOG_CALL_FAIL_INFO("ResponderAuthToken::Generate", res);
        if (res != NRF_SUCCESS)
        {
            NRF_LOG_INFO("BLE-PKAP: Pairing failed - Unable to generate responder auth token");
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
        NRF_LOG_CALL_FAIL_INFO("sd_ble_gatts_value_set", res);
        if (res != NRF_SUCCESS)
        {
            NRF_LOG_INFO("BLE-PKAP: Pairing failed - Unable to publish responder auth token");
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
        ret_code_t res;
        ble_gatts_value_t value;
        memset(&value, 0, sizeof(value));
        value.len = 0;
        value.p_value = (uint8_t *)&value;
        res = sd_ble_gatts_value_set(BLE_GAP_ADV_SET_HANDLE_NOT_SET, sBLEPKAPAuthCharHandles.value_handle, &value);
        NRF_LOG_CALL_FAIL_INFO("sd_ble_gatts_value_set", res);
    }

    NRF_LOG_INFO("BLE-PKAP: Auth state cleared");
}

#endif // defined(SOFTDEVICE_PRESENT) && SOFTDEVICE_PRESENT
