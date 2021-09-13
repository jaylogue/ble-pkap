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
 *         main function for the ble-pkap example embedded application.
 */

#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

#include <sdk_common.h>
#include <boards.h>
#include <app_timer.h>
#include <mem_manager.h>
#include <nrf_pwr_mgmt.h>
#include <nrf_drv_clock.h>
#include <nrf_delay.h>
#include <app_button.h>

#if NRF_LOG_ENABLED
#include <nrf_log.h>
#include <nrf_log_ctrl.h>
#include <nrf_log_default_backends.h>
#endif // NRF_LOG_ENABLED

#if NRF_CRYPTO_ENABLED
#include <nrf_crypto.h>
#endif

#include <SimpleBLEApp.h>
#include <LEDButtonService.h>
#include <BLEPKAPService.h>
#include <BLEEventLogger.h>
#include <nRF5SysTime.h>
#include <nRF5Utils.h>
#include <FunctExitUtils.h>

using namespace nrf5utils;

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

APP_TIMER_DEF(sStatusLEDTimer);

static void StatusLEDTimerHandler(void * context)
{
    nrf_gpio_pin_toggle(APP_STATUS_LED_PIN);
}

void SimpleBLEApp::Event::OnAdvertisingStarted(void)
{
    app_timer_stop(sStatusLEDTimer);
    app_timer_start(sStatusLEDTimer, APP_TIMER_TICKS(APP_STATUS_LED_BLINK_INTERVAL), NULL);
}

void SimpleBLEApp::Event::OnConnectionEstablished(uint16_t conHandle, const ble_gap_evt_connected_t * conEvent)
{
    app_timer_stop(sStatusLEDTimer);
    nrf_gpio_pin_clear(APP_STATUS_LED_PIN);
}

void SimpleBLEApp::Event::OnConnectionTerminated(uint16_t conHandle, const ble_gap_evt_disconnected_t * disconEvent)
{
    app_timer_stop(sStatusLEDTimer);
    nrf_gpio_pin_set(APP_STATUS_LED_PIN);
}

void LEDButtonService::Event::OnLEDWrite(bool setOn)
{
    nrf_gpio_pin_write(APP_UI_LED_PIN, setOn ? 0 : 1);
}

bool BLEPKAPService::Callback::IsKnownPeerKeyId(uint16_t keyId)
{
    return keyId == kTrustedPeerKeyId;
}

ret_code_t BLEPKAPService::Callback::GetPeerPublicKey(uint16_t keyId, const uint8_t * & key, size_t & keySize)
{
    ret_code_t res = NRF_SUCCESS;

    VerifyOrExit(keyId == kTrustedPeerKeyId, res = NRF_ERROR_NOT_FOUND);
    
    key = kTrustedPeerPubKey;
    keySize = sizeof(kTrustedPeerPubKey);

exit:
    return res;
}

int main(void)
{
    ret_code_t res;

#if JLINK_MMD
    NVIC_SetPriority(DebugMonitor_IRQn, _PRIO_APP_LOW);
#endif

    // Initialize clock driver.
    res = nrf_drv_clock_init();
    APP_ERROR_CHECK(res);

    // Start the low-frequency clock and wait for it to be ready.
    nrf_drv_clock_lfclk_request(NULL);
    while (!nrf_clock_lf_is_running()) { }

#if NRF_LOG_ENABLED

    // Initialize logging component
#if NRF_LOG_USES_TIMESTAMP
    res = NRF_LOG_INIT(SysTime::GetSystemTime_MS32, 1000);
#else
    res = NRF_LOG_INIT(NULL, 0);
#endif
    APP_ERROR_CHECK(res);
    NRF_LOG_DEFAULT_BACKENDS_INIT();

#endif // NRF_LOG_ENABLED

    NRF_LOG_INFO("==================================================");
    NRF_LOG_INFO("ble-pkap starting");
    NRF_LOG_INFO("==================================================");

    // Initialize the app_timer module.
    res = app_timer_init();
    NRF_LOG_CALL_FAIL_INFO("app_timer_init", res);
    APP_ERROR_CHECK(res);

    // Initialize the SysTime module.
    res = SysTime::Init();
    NRF_LOG_CALL_FAIL_INFO("SysTime::Init", res);
    APP_ERROR_CHECK(res);

    // Initialize the power management module.
    res = nrf_pwr_mgmt_init();
    NRF_LOG_CALL_FAIL_INFO("nrf_pwr_mgmt_init", res);
    APP_ERROR_CHECK(res);

    // Initialize the nRF5 SDK Memory Manager
    res = nrf_mem_init();
    NRF_LOG_CALL_FAIL_INFO("nrf_mem_init", res);
    APP_ERROR_CHECK(res);

#if NRF_CRYPTO_ENABLED

    // Initialize the nrf_crypto library.
    res = nrf_crypto_init();
    NRF_LOG_CALL_FAIL_INFO("nrf_crypto_init", res);
    APP_ERROR_CHECK(res);

#endif // NRF_CRYPTO_ENABLED

    res = SimpleBLEApp::Init();
    NRF_LOG_CALL_FAIL_INFO("SimpleBLEApp::Init", res);
    APP_ERROR_CHECK(res);

    res = BLEEventLogger::Init();
    NRF_LOG_CALL_FAIL_INFO("BLEEventLogger::Init", res);
    APP_ERROR_CHECK(res);

    res = LEDButtonService::Init();
    NRF_LOG_CALL_FAIL_INFO("LEDButtonService::Init", res);
    APP_ERROR_CHECK(res);

    res = BLEPKAPService::Init(kDeviceKeyId, kDevicePrivKey, sizeof(kDevicePrivKey));
    NRF_LOG_CALL_FAIL_INFO("BLEPKAPService::Init", res);
    APP_ERROR_CHECK(res);

    // Create and start a timer to toggle the status LED
    res = app_timer_create(&sStatusLEDTimer, APP_TIMER_MODE_REPEATED, StatusLEDTimerHandler);
    NRF_LOG_CALL_FAIL_INFO("app_timer_create", res);
    APP_ERROR_CHECK(res);

    // Initialize status and UI LED GPIOs
    nrf_gpio_cfg_output(APP_STATUS_LED_PIN);
    nrf_gpio_pin_set(APP_STATUS_LED_PIN);
    nrf_gpio_cfg_output(APP_UI_LED_PIN);
    nrf_gpio_pin_set(APP_UI_LED_PIN);

    // Initialize the UI button GPIO and the app_button library.
    nrf_gpio_cfg_input(APP_UI_BUTTON_PIN, APP_UI_BUTTON_PULL_CONFIG);
    static app_button_cfg_t sButtonConfigs[] =
    {
        { APP_UI_BUTTON_PIN, APP_UI_BUTTON_ACTIVE_STATE, APP_UI_BUTTON_PULL_CONFIG, LEDButtonService::ButtonEventHandler }
    };
    res = app_button_init(sButtonConfigs, ARRAY_SIZE(sButtonConfigs), APP_BUTTON_DETECTION_DELAY);
    NRF_LOG_CALL_FAIL_INFO("app_button_init", res);
    APP_ERROR_CHECK(res);
    res = app_button_enable();
    NRF_LOG_CALL_FAIL_INFO("app_button_enable", res);
    APP_ERROR_CHECK(res);

    NRF_LOG_INFO("System initialization complete");

#if NRF_LOG_ENABLED && NRF_LOG_LEVEL >= NRF_LOG_SEVERITY_INFO
    LogHeapStats();
#endif

    res = SimpleBLEApp::StartAdvertising();
    NRF_LOG_CALL_FAIL_INFO("SimpleBLEApp::StartAdvertising", res);
    APP_ERROR_CHECK(res);

    NRF_LOG_INFO("Starting main loop");

    while (true)
    {
#if NRF_LOG_DEFERRED
        while (NRF_LOG_PROCESS())
            ;
#endif

        res = SimpleBLEApp::RunMainLoopActions();
        NRF_LOG_CALL_FAIL_INFO("SimpleBLEApp::RunMainLoopActions", res);
        APP_ERROR_CHECK(res);

        res = BLEPKAPService::RunMainLoopActions();
        NRF_LOG_CALL_FAIL_INFO("BLEPKAPService::RunMainLoopActions", res);
        APP_ERROR_CHECK(res);

        nrf_pwr_mgmt_run();
    }
}
