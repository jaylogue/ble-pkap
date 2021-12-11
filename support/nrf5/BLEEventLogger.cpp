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
 *         Support for automatic logging of Nordic SoftDevice BLE events.
 */

#include <sdk_common.h>

#if !defined(SOFTDEVICE_PRESENT) || !SOFTDEVICE_PRESENT
#error BLEEventLogger requires SoftDevice to be enabled
#endif // defined(SOFTDEVICE_PRESENT) && SOFTDEVICE_PRESENT

#include <inttypes.h>
#include <stdio.h>

#include <ble.h>

#if NRF_LOG_ENABLED
#include <nrf_log.h>
#endif // NRF_LOG_ENABLED

#include <BLEEventLogger.h>
#include <LESCOOB.h>
#include <nRF5Utils.h>
#include <FunctExitUtils.h>

namespace nrf5utils {

namespace {

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

ret_code_t BLEEventLogger::Init(void)
{
#if NRF_LOG_ENABLED && NRF_LOG_LEVEL >= NRF_LOG_SEVERITY_INFO

    // Static declaration of BLE observer for BLEEventLogger class.
    NRF_SDH_BLE_OBSERVER(sBLEEventLogger_BLEObserver, BLE_EVENT_LOGGER_OBSERVER_PRIO, BLEEventLogger::HandleBLEEvent, NULL);

#endif // NRF_LOG_ENABLED && NRF_LOG_LEVEL >= NRF_LOG_SEVERITY_INFO

    return NRF_SUCCESS;
}

void BLEEventLogger::HandleBLEEvent(ble_evt_t const * bleEvent, void * context)
{
#if NRF_LOG_ENABLED && NRF_LOG_LEVEL >= NRF_LOG_SEVERITY_INFO

    uint16_t conHandle = bleEvent->evt.gap_evt.conn_handle;
    const char * eventName = NULL;
    char eventNameBuf[32];

    switch (bleEvent->header.evt_id)
    {
    case BLE_GAP_EVT_CONNECTED:
        NRF_LOG_INFO("BLE connection established (con %" PRIu16 ")", conHandle);
        return;

    case BLE_GAP_EVT_DISCONNECTED:
        NRF_LOG_INFO("BLE connection terminated (con %" PRIu16 ", reason 0x%02" PRIx8 ")", conHandle, bleEvent->evt.gap_evt.params.disconnected.reason);
        return;

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
        return;
    }

    case BLE_GAP_EVT_AUTH_KEY_REQUEST:
    {
        const ble_gap_evt_auth_key_request_t * authKeyReq = &bleEvent->evt.gap_evt.params.auth_key_request;

        NRF_LOG_INFO("BLE_GAP_EVT_AUTH_KEY_REQUEST received (con %" PRIu16 ")", conHandle);
        NRF_LOG_INFO("    key_type: %" PRIu8, authKeyReq->key_type);
        return;
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

        return;
    }

    case BLE_GAP_EVT_AUTH_STATUS:
    {
        const ble_gap_evt_auth_status_t * authStatus = &bleEvent->evt.gap_evt.params.auth_status;

        NRF_LOG_INFO("BLE_GAP_EVT_AUTH_STATUS received (con %" PRIu16 ")", conHandle);
        NRF_LOG_INFO("    auth_status: 0x%02" PRIX8 " - %s", authStatus->auth_status, GetSecStatusStr(authStatus->auth_status));
        const char *errorSrcStr;
        switch (authStatus->error_src)
        {
        case BLE_GAP_SEC_STATUS_SOURCE_LOCAL:
            errorSrcStr = " (local)";
            break;
        case BLE_GAP_SEC_STATUS_SOURCE_REMOTE:
            errorSrcStr = " (remote)";
            break;
        default:
            errorSrcStr = "";
            break;
        }
        NRF_LOG_INFO("    error_src: 0x%02" PRIX8 "%s", authStatus->error_src, errorSrcStr);
        NRF_LOG_INFO("    bonded: %" PRId8, authStatus->bonded);
        NRF_LOG_INFO("    lesc: %" PRId8, authStatus->lesc);
        NRF_LOG_INFO("    sec mode 1, level 1: %" PRId8, authStatus->sm1_levels.lv1);
        NRF_LOG_INFO("    sec mode 1, level 2: %" PRId8, authStatus->sm1_levels.lv2);
        NRF_LOG_INFO("    sec mode 1, level 3: %" PRId8, authStatus->sm1_levels.lv3);
        NRF_LOG_INFO("    sec mode 1, level 4: %" PRId8, authStatus->sm1_levels.lv4);
        NRF_LOG_INFO("    sec mode 2, level 1: %" PRId8, authStatus->sm2_levels.lv1);
        NRF_LOG_INFO("    sec mode 2, level 2: %" PRId8, authStatus->sm2_levels.lv2);
        return;
    }

    case BLE_GAP_EVT_CONN_SEC_UPDATE:
    {
        const ble_gap_evt_conn_sec_update_t * connSecUpdate = &bleEvent->evt.gap_evt.params.conn_sec_update;

        NRF_LOG_INFO("BLE_GAP_EVT_CONN_SEC_UPDATE received (con %" PRIu16 ")", conHandle);
        NRF_LOG_INFO("    sec mode: 0x%02" PRIX8, connSecUpdate->conn_sec.sec_mode.sm);
        NRF_LOG_INFO("    sec level: 0x%02" PRIX8, connSecUpdate->conn_sec.sec_mode.lv);
        NRF_LOG_INFO("    encr_key_size: %" PRId8, connSecUpdate->conn_sec.encr_key_size);
        return;
    }

    case BLE_GATTS_EVT_TIMEOUT:
        NRF_LOG_INFO("BLE GATT Server timeout (con %" PRIu16 ")", bleEvent->evt.gatts_evt.conn_handle);
        return;

    case BLE_EVT_USER_MEM_REQUEST                   : eventName = "BLE_EVT_USER_MEM_REQUEST"; break;
    case BLE_EVT_USER_MEM_RELEASE                   : eventName = "BLE_EVT_USER_MEM_RELEASE"; break;

    case BLE_GAP_EVT_SEC_INFO_REQUEST               : eventName = "BLE_GAP_EVT_SEC_INFO_REQUEST"; break;
    case BLE_GAP_EVT_PASSKEY_DISPLAY                : eventName = "BLE_GAP_EVT_PASSKEY_DISPLAY"; break;
    case BLE_GAP_EVT_KEY_PRESSED                    : eventName = "BLE_GAP_EVT_KEY_PRESSED"; break;
    case BLE_GAP_EVT_CONN_PARAM_UPDATE              : eventName = "BLE_GAP_EVT_CONN_PARAM_UPDATE"; break;
    case BLE_GAP_EVT_TIMEOUT                        : eventName = "BLE_GAP_EVT_TIMEOUT"; break;
    case BLE_GAP_EVT_RSSI_CHANGED                   : eventName = "BLE_GAP_EVT_RSSI_CHANGED"; break;
    case BLE_GAP_EVT_ADV_REPORT                     : eventName = "BLE_GAP_EVT_ADV_REPORT"; break;
    case BLE_GAP_EVT_SEC_REQUEST                    : eventName = "BLE_GAP_EVT_SEC_REQUEST"; break;
    case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST      : eventName = "BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST"; break;
    case BLE_GAP_EVT_SCAN_REQ_REPORT                : eventName = "BLE_GAP_EVT_SCAN_REQ_REPORT"; break;
    case BLE_GAP_EVT_PHY_UPDATE_REQUEST             : eventName = "BLE_GAP_EVT_PHY_UPDATE_REQUEST"; break;
    case BLE_GAP_EVT_PHY_UPDATE                     : eventName = "BLE_GAP_EVT_PHY_UPDATE"; break;
    case BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST     : eventName = "BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST"; break;
    case BLE_GAP_EVT_DATA_LENGTH_UPDATE             : eventName = "BLE_GAP_EVT_DATA_LENGTH_UPDATE"; break;
    case BLE_GAP_EVT_QOS_CHANNEL_SURVEY_REPORT      : eventName = "BLE_GAP_EVT_QOS_CHANNEL_SURVEY_REPORT"; break;
    case BLE_GAP_EVT_ADV_SET_TERMINATED             : eventName = "BLE_GAP_EVT_ADV_SET_TERMINATED"; break;

    case BLE_GATTS_EVT_WRITE                        : eventName = "BLE_GATTS_EVT_WRITE"; break;
    case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST         : eventName = "BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST"; break;
    case BLE_GATTS_EVT_SYS_ATTR_MISSING             : eventName = "BLE_GATTS_EVT_SYS_ATTR_MISSING"; break;
    case BLE_GATTS_EVT_HVC                          : eventName = "BLE_GATTS_EVT_HVC"; break;
    case BLE_GATTS_EVT_SC_CONFIRM                   : eventName = "BLE_GATTS_EVT_SC_CONFIRM"; break;
    case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST         : eventName = "BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST"; break;
    case BLE_GATTS_EVT_HVN_TX_COMPLETE              : eventName = "BLE_GATTS_EVT_HVN_TX_COMPLETE"; break;

    case BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP           : eventName = "BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP"; break;
    case BLE_GATTC_EVT_REL_DISC_RSP                 : eventName = "BLE_GATTC_EVT_REL_DISC_RSP"; break;
    case BLE_GATTC_EVT_CHAR_DISC_RSP                : eventName = "BLE_GATTC_EVT_CHAR_DISC_RSP"; break;
    case BLE_GATTC_EVT_DESC_DISC_RSP                : eventName = "BLE_GATTC_EVT_DESC_DISC_RSP"; break;
    case BLE_GATTC_EVT_ATTR_INFO_DISC_RSP           : eventName = "BLE_GATTC_EVT_ATTR_INFO_DISC_RSP"; break;
    case BLE_GATTC_EVT_CHAR_VAL_BY_UUID_READ_RSP    : eventName = "BLE_GATTC_EVT_CHAR_VAL_BY_UUID_READ_RSP"; break;
    case BLE_GATTC_EVT_READ_RSP                     : eventName = "BLE_GATTC_EVT_READ_RSP"; break;
    case BLE_GATTC_EVT_CHAR_VALS_READ_RSP           : eventName = "BLE_GATTC_EVT_CHAR_VALS_READ_RSP"; break;
    case BLE_GATTC_EVT_WRITE_RSP                    : eventName = "BLE_GATTC_EVT_WRITE_RSP"; break;
    case BLE_GATTC_EVT_HVX                          : eventName = "BLE_GATTC_EVT_HVX"; break;
    case BLE_GATTC_EVT_EXCHANGE_MTU_RSP             : eventName = "BLE_GATTC_EVT_EXCHANGE_MTU_RSP"; break;
    case BLE_GATTC_EVT_TIMEOUT                      : eventName = "BLE_GATTC_EVT_TIMEOUT"; break;
    case BLE_GATTC_EVT_WRITE_CMD_TX_COMPLETE        : eventName = "BLE_GATTC_EVT_WRITE_CMD_TX_COMPLETE"; break;

    case BLE_L2CAP_EVT_CH_SETUP_REQUEST             : eventName = "BLE_L2CAP_EVT_CH_SETUP_REQUEST"; break;
    case BLE_L2CAP_EVT_CH_SETUP_REFUSED             : eventName = "BLE_L2CAP_EVT_CH_SETUP_REFUSED"; break;
    case BLE_L2CAP_EVT_CH_SETUP                     : eventName = "BLE_L2CAP_EVT_CH_SETUP"; break;
    case BLE_L2CAP_EVT_CH_RELEASED                  : eventName = "BLE_L2CAP_EVT_CH_RELEASED"; break;
    case BLE_L2CAP_EVT_CH_SDU_BUF_RELEASED          : eventName = "BLE_L2CAP_EVT_CH_SDU_BUF_RELEASED"; break;
    case BLE_L2CAP_EVT_CH_CREDIT                    : eventName = "BLE_L2CAP_EVT_CH_CREDIT"; break;
    case BLE_L2CAP_EVT_CH_RX                        : eventName = "BLE_L2CAP_EVT_CH_RX"; break;
    case BLE_L2CAP_EVT_CH_TX                        : eventName = "BLE_L2CAP_EVT_CH_TX"; break;

    default:
        snprintf(eventNameBuf, sizeof(eventNameBuf), "BLE event 0x%04" PRIX16, bleEvent->header.evt_id);
        eventName = eventNameBuf;
        break;
    }

    NRF_LOG_INFO("%s received (con %" PRIu16 ")", eventName, conHandle);

#endif // NRF_LOG_ENABLED && NRF_LOG_LEVEL >= NRF_LOG_SEVERITY_INFO
}

} // namespace nrf5utils
