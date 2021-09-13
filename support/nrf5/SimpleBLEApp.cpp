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
 *         A simple BLE application based on the Nordic SoftDevice and nRF5 SDK.
 */

#include <sdk_common.h>

#if defined(SOFTDEVICE_PRESENT) && SOFTDEVICE_PRESENT

#include <inttypes.h>
#include <stdio.h>

#include <nrf_sdh.h>
#include <nrf_sdh_ble.h>
#include <nrf_sdh_soc.h>
#include <ble.h>
#include <ble_gap.h>
#include <ble_gattc.h>
#include <ble_advdata.h>
#include <ble_srv_common.h>
#include <nrf_ble_gatt.h>
#include <ble_conn_state.h>

#if NRF_BLE_LESC_ENABLED
#include <nrf_ble_lesc.h>
#endif

#if NRF_LOG_ENABLED
#include <nrf_log.h>
#endif // NRF_LOG_ENABLED

#include <SimpleBLEApp.h>
#include <LESCOOB.h>
#include <nRF5Utils.h>
#include <FunctExitUtils.h>

namespace nrf5utils {

namespace {

NRF_BLE_GATT_DEF(sGATTModule);
uint8_t sAdvHandle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;
uint8_t sEncodedAdvDataBuf[BLE_GAP_ADV_SET_DATA_SIZE_MAX];
uint16_t sEncodedAdvDataLen;
uint32_t sAdvRateMS = SIMPLE_BLE_APP_DEFAULT_ADV_RATE;
bool sAdvEnabled;
ble_uuid_t sAdvServiceUUID;
ble_advdata_manuf_data_t sAdvManufData;
char sDevName[SIMPLE_BLE_APP_DEVICE_NAME_MAX_LENGTH + 1];

#if SIMPLE_BLE_APP_LESC_PAIRING && !SIMPLE_BLE_APP_EXTERNAL_PAIRING
ble_gap_lesc_p256_pk_t sPeerLESCPubKey;
#endif

} // unnamed namespace

ret_code_t SimpleBLEApp::Init(void)
{
    ret_code_t res;

    // Static declaration of BLE observer for SimpleBLEApp class.
    NRF_SDH_BLE_OBSERVER(sSimpleBLEApp_BLEObserver, SIMPLE_BLE_APP_OBSERVER_PRIO, SimpleBLEApp::HandleBLEEvent, NULL);

    NRF_LOG_INFO("Initializing BLE application");

#if SIMPLE_BLE_APP_INIT_SOFTDEVICE

    NRF_LOG_INFO("Enabling SoftDevice");

    res = nrf_sdh_enable_request();
    NRF_LOG_CALL_FAIL_INFO("nrf_sdh_enable_request", res);
    SuccessOrExit(res);

    NRF_LOG_INFO("Waiting for SoftDevice to be enabled");

    while (!nrf_sdh_is_enabled()) { }

    NRF_LOG_INFO("SoftDevice enable complete");

    {
        uint32_t appRAMStart = 0;

        // Configure the BLE stack using the default settings.
        // Fetch the start address of the application RAM.
        res = nrf_sdh_ble_default_cfg_set(BLE_CONN_CONFIG_TAG, &appRAMStart);
        NRF_LOG_CALL_FAIL_INFO("nrf_sdh_ble_default_cfg_set", res);
        SuccessOrExit(res);

        // Enable BLE stack.
        res = nrf_sdh_ble_enable(&appRAMStart);
        NRF_LOG_CALL_FAIL_INFO("nrf_sdh_ble_enable", res);
        SuccessOrExit(res);
    }

#endif // SIMPLE_BLE_APP_INIT_SOFTDEVICE

    // Initialize the nRF5 GATT module and set the allowable GATT MTU and GAP packet sizes
    // based on compile-time config values.
    res = nrf_ble_gatt_init(&sGATTModule, NULL);
    NRF_LOG_CALL_FAIL_INFO("nrf_ble_gatt_init", res);
    SuccessOrExit(res);
    res = nrf_ble_gatt_att_mtu_periph_set(&sGATTModule, NRF_SDH_BLE_GATT_MAX_MTU_SIZE);
    NRF_LOG_CALL_FAIL_INFO("nrf_ble_gatt_att_mtu_periph_set", res);
    SuccessOrExit(res);
    res = nrf_ble_gatt_data_length_set(&sGATTModule, BLE_CONN_HANDLE_INVALID, NRF_SDH_BLE_GAP_DATA_LENGTH);
    NRF_LOG_CALL_FAIL_INFO("nrf_ble_gatt_data_length_set", res);
    SuccessOrExit(res);

    // Initialize the connection state module.
    ble_conn_state_init();

#if NRF_BLE_LESC_ENABLED && !SIMPLE_BLE_APP_EXTERNAL_PAIRING

    // Initalize the BLE LESC module.
    res = nrf_ble_lesc_init();
    NRF_LOG_CALL_FAIL_INFO("nrf_ble_lesc_init", res);
    SuccessOrExit(res);

#endif // NRF_BLE_LESC_ENABLED && !SIMPLE_BLE_APP_EXTERNAL_PAIRING

    // Setup the default BLE device name.
    res = SetDeviceName(NULL);
    SuccessOrExit(res);

exit:
    return res;
}

ret_code_t SimpleBLEApp::SetDeviceName(const char * devName, bool makeUnique)
{
    ret_code_t res = NRF_SUCCESS;
    ble_gap_conn_sec_mode_t secMode;

    if (devName == NULL)
    {
        devName = SIMPLE_BLE_APP_DEFAULT_DEVICE_NAME;
        makeUnique = (SIMPLE_BLE_APP_UNIQUE_DEVICE_NAME != 0);
    }

    if (makeUnique)
    {
        ble_gap_addr_t devAddr;

        // Get the device's BLE MAC address
        res = sd_ble_gap_addr_get(&devAddr);
        NRF_LOG_CALL_FAIL_INFO("sd_ble_gap_addr_get", res);
        SuccessOrExit(res);

        // Form a unique device name appending the last digits of the MAC address.
        snprintf(sDevName, sizeof(sDevName), "%.*s%02" PRIX8 "%02" PRIX8,
                sizeof(sDevName) - 5, devName, devAddr.addr[1], devAddr.addr[0]);
        sDevName[sizeof(sDevName) - 1] = 0;
    }

    else
    {
        strncpy(sDevName, devName, sizeof(sDevName) -1);
        sDevName[sizeof(sDevName) - 1] = 0;
    }

    // Do not allow device name characteristic to be changed
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&secMode);

    // Configure the device name within the BLE soft device.
    res = sd_ble_gap_device_name_set(&secMode, (const uint8_t *)sDevName, strlen(sDevName));
    NRF_LOG_CALL_FAIL_INFO("sd_ble_gap_device_name_set", res);
    SuccessOrExit(res);

exit:
    return res;
}

void SimpleBLEApp::SetAdvertisingRate(uint32_t advRateMS)
{
    sAdvRateMS = advRateMS;
}

uint32_t SimpleBLEApp::GetAdvertisingRate(void)
{
    return sAdvRateMS;
}

void SimpleBLEApp::SetAdvertisedServiceUUID(ble_uuid_t advServiceUUID)
{
    sAdvServiceUUID = advServiceUUID;
}

void SimpleBLEApp::ClearAdvertisedServiceUUID(void)
{
    sAdvServiceUUID.type = BLE_UUID_TYPE_UNKNOWN;
}

void SimpleBLEApp::SetAdvertisedManufacturingData(const ble_advdata_manuf_data_t & manufData)
{
    sAdvManufData = manufData;
}

void SimpleBLEApp::ClearAdvertisedManufacturingData(void)
{
    sAdvManufData.data.p_data = NULL;
}

ret_code_t SimpleBLEApp::StartAdvertising(void)
{
    ret_code_t res;
    ble_advdata_t advData;
    ble_gap_adv_data_t gapAdvData;
    ble_gap_adv_params_t gapAdvParams;

    if (sAdvHandle != BLE_GAP_ADV_SET_HANDLE_NOT_SET)
    {
        // Instruct the SoftDevice to stop advertising.
        // Ignore any error indicating that advertising is already stopped.  This case is arises
        // when a connection is established, which causes the SoftDevice to immediately cease
        // advertising.
        res = sd_ble_gap_adv_stop(sAdvHandle);
        res = (res != NRF_ERROR_INVALID_STATE) ? res : NRF_SUCCESS;
        NRF_LOG_CALL_FAIL_INFO("sd_ble_gap_adv_stop", res);
        SuccessOrExit(res);

        // Force the SoftDevice to relinquish its references to the buffers containing the advertising
        // data.  This ensures the SoftDevice is not accessing these buffers while we are encoding
        // new advertising data into them.
        res = sd_ble_gap_adv_set_configure(&sAdvHandle, NULL, NULL);
        NRF_LOG_CALL_FAIL_INFO("sd_ble_gap_adv_set_configure", res);
        SuccessOrExit(res);
    }

    // Form the contents of the advertising packet.
    memset(&advData, 0, sizeof(advData));
    advData.name_type = SIMPLE_BLE_APP_ADV_NAME_TYPE;
    advData.include_appearance = false;
    advData.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    if (sAdvServiceUUID.type != BLE_UUID_TYPE_UNKNOWN)
    {
        advData.uuids_complete.uuid_cnt = 1;
        advData.uuids_complete.p_uuids = &sAdvServiceUUID;
    }
    if (sAdvManufData.data.p_data != NULL)
    {
        advData.p_manuf_specific_data = &sAdvManufData;
    }
    sEncodedAdvDataLen = sizeof(sEncodedAdvDataBuf);
    res = ble_advdata_encode(&advData, sEncodedAdvDataBuf, &sEncodedAdvDataLen);
    NRF_LOG_CALL_FAIL_INFO("ble_advdata_encode", res);
    SuccessOrExit(res);

    // Setup parameters controlling how advertising will happen.
    memset(&gapAdvParams, 0, sizeof(gapAdvParams));
    gapAdvParams.properties.type = 
#if NRF_SDH_BLE_PERIPHERAL_LINK_COUNT > 0
        BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
#else
        BLE_GAP_ADV_TYPE_NONCONNECTABLE_SCANNABLE_UNDIRECTED;
#endif
    gapAdvParams.primary_phy     = BLE_GAP_PHY_1MBPS;
    gapAdvParams.duration        = BLE_GAP_ADV_TIMEOUT_GENERAL_UNLIMITED;
    gapAdvParams.filter_policy   = BLE_GAP_ADV_FP_ANY;
    gapAdvParams.interval        = MSEC_TO_UNITS(sAdvRateMS, UNIT_0_625_MS);

    // Configure an "advertising set" in the BLE soft device with the given data and parameters.
    // If the advertising set doesn't exist, this call will create it and return its handle.
    memset(&gapAdvData, 0, sizeof(gapAdvData));
    gapAdvData.adv_data.p_data = sEncodedAdvDataBuf;
    gapAdvData.adv_data.len = sEncodedAdvDataLen;
    res = sd_ble_gap_adv_set_configure(&sAdvHandle, &gapAdvData, &gapAdvParams);
    NRF_LOG_CALL_FAIL_INFO("sd_ble_gap_adv_set_configure", res);
    SuccessOrExit(res);

#if NRF_LOG_ENABLED && NRF_LOG_LEVEL >= NRF_LOG_SEVERITY_INFO

    {
        ble_gap_addr_t devAddr;
        char devAddrStr[6*3+1];
        sd_ble_gap_addr_get(&devAddr);
        snprintf(devAddrStr, sizeof(devAddrStr), "%02" PRIX8 ":%02" PRIX8 ":%02" PRIX8 ":%02" PRIX8 ":%02" PRIX8 ":%02" PRIX8,
                 devAddr.addr[5], devAddr.addr[4], devAddr.addr[3], devAddr.addr[2], devAddr.addr[1], devAddr.addr[0]);
        NRF_LOG_INFO("Starting BLE advertising (device name: '%s', MAC addr: %s)", sDevName, devAddrStr);
    }

#endif

    // Instruct the soft device to start advertising using the configured advertising set.
    res = sd_ble_gap_adv_start(sAdvHandle, BLE_CONN_CONFIG_TAG);
    NRF_LOG_CALL_FAIL_INFO("sd_ble_gap_adv_start", res);
    SuccessOrExit(res);

    sAdvEnabled = true;

    // Let the application know that advertising has started.
    if (Event::OnAdvertisingStarted)
    {
        Event::OnAdvertisingStarted();
    }

exit:
    return res;
}

ret_code_t SimpleBLEApp::StopAdvertising(void)
{
    ret_code_t res = NRF_SUCCESS;

    if (sAdvEnabled)
    {
        NRF_LOG_INFO("Stopping BLE advertising");

        sAdvEnabled = false;

        // Instruct the SoftDevice to stop advertising.
        // Ignore any error indicating that advertising is already stopped.  This case is arises
        // when a connection is established, which causes the SoftDevice to immediately cease
        // advertising.
        res = sd_ble_gap_adv_stop(sAdvHandle);
        res = (res != NRF_ERROR_INVALID_STATE) ? res : NRF_SUCCESS;
        NRF_LOG_CALL_FAIL_INFO("sd_ble_gap_adv_stop", res);
        SuccessOrExit(res);

        // Let the application know that advertising has stopped.
        if (Event::OnAdvertisingStopped)
        {
            Event::OnAdvertisingStopped();
        }
    }

exit:
    return res;
}

bool SimpleBLEApp::IsAdvertising(void)
{
    return sAdvEnabled;
}

ret_code_t SimpleBLEApp::RunMainLoopActions(void)
{
    ret_code_t res = NRF_SUCCESS;

#if NRF_BLE_LESC_ENABLED && !SIMPLE_BLE_APP_EXTERNAL_PAIRING

    res = nrf_ble_lesc_request_handler();
    NRF_LOG_CALL_FAIL_INFO("nrf_ble_lesc_request_handler", res);

#endif // NRF_BLE_LESC_ENABLED && !SIMPLE_BLE_APP_EXTERNAL_PAIRING

    return res;
}

void SimpleBLEApp::HandleBLEEvent(ble_evt_t const * bleEvent, void * context)
{
    ret_code_t res;
    uint16_t conHandle = bleEvent->evt.gap_evt.conn_handle;

    UNUSED_VARIABLE(res);
    UNUSED_VARIABLE(conHandle);

    switch (bleEvent->header.evt_id)
    {
    case BLE_GAP_EVT_CONNECTED:

        // Invoke the application's event handler, if defined.
        if (Event::OnConnectionEstablished)
        {
            Event::OnConnectionEstablished(conHandle, &bleEvent->evt.gap_evt.params.connected);
        }

        // Re-start advertising if more than one peripheral connection allowed and not at the maximum
        // number of peripheral connections.
#if NRF_SDH_BLE_PERIPHERAL_LINK_COUNT > 1
        if (sAdvEnabled && ble_conn_state_peripheral_conn_count() < NRF_SDH_BLE_PERIPHERAL_LINK_COUNT)
        {
            StartAdvertising();
        }
#endif

        break;

    case BLE_GAP_EVT_DISCONNECTED:

#if NRF_LOG_ENABLED && NRF_LOG_LEVEL >= NRF_LOG_SEVERITY_INFO
        LogHeapStats();
#endif

        // Invoke the application's event handler, if defined.
        if (Event::OnConnectionTerminated)
        {
            Event::OnConnectionTerminated(conHandle, &bleEvent->evt.gap_evt.params.disconnected);
        }

        // Re-start advertising if not at the maximum number of peripheral connections.
        if (sAdvEnabled && ble_conn_state_peripheral_conn_count() < NRF_SDH_BLE_PERIPHERAL_LINK_COUNT)
        {
            StartAdvertising();
        }

        break;

    case BLE_GATTS_EVT_SYS_ATTR_MISSING:
        res = sd_ble_gatts_sys_attr_set(bleEvent->evt.gatts_evt.conn_handle, NULL, 0, 0);
        NRF_LOG_CALL_FAIL_INFO("sd_ble_gatts_sys_attr_set", res);
        break;

#if !SIMPLE_BLE_APP_EXTERNAL_PAIRING

    case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
    {
        const ble_gap_evt_sec_params_request_t * secParamsReq = &bleEvent->evt.gap_evt.params.sec_params_request;

        uint8_t secStatus;

        ble_gap_sec_params_t secParamsReply;
        memset(&secParamsReply, 0, sizeof(secParamsReply));

        ble_gap_sec_keyset_t keySet;
        memset(&keySet, 0, sizeof(keySet));

#if SIMPLE_BLE_APP_LESC_PAIRING

        if (!secParamsReq->peer_params.lesc)
        {
            secStatus = BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP;
        }
        else
        {
            // Instruct the SoftDevice to perform LESC 'Just Works' pairing with the peer.
            secStatus = BLE_GAP_SEC_STATUS_SUCCESS;
            secParamsReply.lesc = 1;
            secParamsReply.mitm = 0;
            secParamsReply.io_caps = BLE_GAP_IO_CAPS_NONE;

            // Demand a key size of 128-bits
            secParamsReply.min_key_size = 16;
            secParamsReply.max_key_size = 16;

            // Supply the local LESC public key to the SoftDevice and provide space to 
            // receive the peer's public key.
            keySet.keys_own.p_pk = nrf_ble_lesc_public_key_get();
            keySet.keys_peer.p_pk = &sPeerLESCPubKey;
        }

#else // SIMPLE_BLE_APP_LESC_PAIRING

        secStatus = BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP;
        NRF_LOG_INFO("Rejecting pairing request");

#endif // SIMPLE_BLE_APP_LESC_PAIRING

        // Invoke the application's event handler, if defined.
        if (Event::OnPairingRequested)
        {
            Event::OnPairingRequested(conHandle, secParamsReq, secStatus, &secParamsReply);
        }

#if NRF_LOG_ENABLED && NRF_LOG_LEVEL >= NRF_LOG_SEVERITY_INFO
        if (secStatus == BLE_GAP_SEC_STATUS_SUCCESS)
        {
            NRF_LOG_INFO("Initiating %s pairing", secParamsReply.lesc ? "LESC" : "Legacy");
            if (secParamsReply.lesc)
            {
                LogLocalLESCPublicKey();
            }
        }
        else
        {
            NRF_LOG_INFO("Rejecting pairing request: %s", GetSecStatusStr(secStatus));
        }
#endif // NRF_LOG_ENABLED && NRF_LOG_LEVEL >= NRF_LOG_SEVERITY_INFO

        // Reply to the SoftDevice's security parameters request
        res = sd_ble_gap_sec_params_reply(conHandle, secStatus, &secParamsReply, &keySet);
        NRF_LOG_CALL_FAIL_INFO("sd_ble_gap_sec_params_reply", res);

        break;
    }

    case BLE_GAP_EVT_AUTH_STATUS:
    {
        const ble_gap_evt_auth_status_t * authStatus = &bleEvent->evt.gap_evt.params.auth_status;

#if NRF_LOG_ENABLED && NRF_LOG_LEVEL >= NRF_LOG_SEVERITY_INFO

        if (authStatus->auth_status == BLE_GAP_SEC_STATUS_SUCCESS)
        {
            NRF_LOG_INFO("%s pairing completed successfully", authStatus->lesc ? "LESC" : "Legacy");
        }
        else
        {
            NRF_LOG_INFO("Pairing failed due to %s error: 0x%02" PRIX8 " : %s", 
                authStatus->error_src == BLE_GAP_SEC_STATUS_SOURCE_LOCAL ? "local" : "remote",
                authStatus->auth_status,
                GetSecStatusStr(authStatus->auth_status));
        }

#endif // NRF_LOG_ENABLED && NRF_LOG_LEVEL >= NRF_LOG_SEVERITY_INFO

        // Invoke the application's event handler, if defined.
        if (Event::OnPairingCompleted)
        {
            Event::OnPairingCompleted(conHandle, authStatus);
        }

        break;
    }

    case BLE_GAP_EVT_AUTH_KEY_REQUEST:
        // NOTE: Since this event is used for legacy pairing, and legacy pairing is unsupported,
        // we respond with a key type of NONE.
        res = sd_ble_gap_auth_key_reply(conHandle, BLE_GAP_AUTH_KEY_TYPE_NONE, NULL);
        NRF_LOG_CALL_FAIL_INFO("sd_ble_gap_auth_key_reply", res);
        break;

#endif // !SIMPLE_BLE_APP_EXTERNAL_PAIRING

    }

#if NRF_BLE_LESC_ENABLED && !SIMPLE_BLE_APP_EXTERNAL_PAIRING
    nrf_ble_lesc_on_ble_evt(bleEvent);
#endif
}

} // namespace nrf5utils

#endif // defined(SOFTDEVICE_PRESENT) && SOFTDEVICE_PRESENT

