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
#include <malloc.h>
#include <inttypes.h>

#include <sdk_common.h>
#include "boards.h"
#include "app_timer.h"
#include "mem_manager.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_drv_clock.h"
#include "nrf_delay.h"
#include "app_button.h"

#if NRF_LOG_ENABLED
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#endif // NRF_LOG_ENABLED

#ifdef SOFTDEVICE_PRESENT
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_sdh_soc.h"
#include "ble_advdata.h"
#include "nrf_ble_lesc.h"
#endif // SOFTDEVICE_PRESENT

#if NRF_CRYPTO_ENABLED
#include "nrf_crypto.h"
#endif

#include "nRF5SysTime.h"
#include "BLEPKAPService.h"

using namespace nrf5utils;

extern "C" size_t GetHeapTotalSize(void);

APP_TIMER_DEF(sStatusLEDTimer);

void OnAdvertisingStarted(void)
{
    app_timer_stop(sStatusLEDTimer);
    app_timer_start(sStatusLEDTimer, APP_TIMER_TICKS(APP_STATUS_LED_SLOW_INTERVAL), NULL);
}

void OnConnectionEstablished(void)
{
    app_timer_stop(sStatusLEDTimer);
    nrf_gpio_pin_clear(APP_STATUS_LED_PIN);
}

void OnConnectionTerminated(void)
{
    app_timer_stop(sStatusLEDTimer);
    nrf_gpio_pin_set(APP_STATUS_LED_PIN);
}

void OnLEDWrite(bool setOn)
{
    nrf_gpio_pin_write(APP_UI_LED_PIN, setOn ? 0 : 1);
}

static void StatusLEDTimerHandler(void * context)
{
    nrf_gpio_pin_toggle(APP_STATUS_LED_PIN);
}

static void ButtonEventHandler(uint8_t buttonPin, uint8_t buttonAction)
{
    if (buttonPin == APP_UI_BUTTON_PIN)
    {
        NRF_LOG_INFO("Button state change: %s", buttonAction == APP_BUTTON_PUSH ? "PRESSED" : "RELEASED");

        BLEPKAPService::UpdateButtonState(buttonAction == APP_BUTTON_PUSH);
    }
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
    NRF_LOG_INFO("ble-pkap example app starting");
    NRF_LOG_INFO("==================================================");

    // Initialize the app_timer module.
    res = app_timer_init();
    if (res != NRF_SUCCESS)
    {
        NRF_LOG_INFO("app_timer_init() failed");
        APP_ERROR_HANDLER(res);
    }

    // Initialize the SysTime module.
    res = SysTime::Init();
    if (res != NRF_SUCCESS)
    {
        NRF_LOG_INFO("SysTime::Init() failed");
        APP_ERROR_HANDLER(res);
    }

    // Initialize the power management module.
    res = nrf_pwr_mgmt_init();
    if (res != NRF_SUCCESS)
    {
        NRF_LOG_INFO("nrf_pwr_mgmt_init() failed");
        APP_ERROR_HANDLER(res);
    }

#if defined(SOFTDEVICE_PRESENT) && SOFTDEVICE_PRESENT

    NRF_LOG_INFO("Enabling SoftDevice");

    res = nrf_sdh_enable_request();
    if (res != NRF_SUCCESS)
    {
        NRF_LOG_INFO("nrf_sdh_enable_request() failed");
        APP_ERROR_HANDLER(res);
    }

    NRF_LOG_INFO("Waiting for SoftDevice to be enabled");

    while (!nrf_sdh_is_enabled()) { }

    NRF_LOG_INFO("SoftDevice enable complete");

#endif // defined(SOFTDEVICE_PRESENT) && SOFTDEVICE_PRESENT

    // Initialize the nRF5 SDK Memory Manager
    res = nrf_mem_init();
    if (res != NRF_SUCCESS)
    {
        NRF_LOG_INFO("nrf_mem_init() failed");
        APP_ERROR_HANDLER(res);
    }

#if NRF_CRYPTO_ENABLED

    // Initialize the nrf_crypto library.
    res = nrf_crypto_init();
    if (res != NRF_SUCCESS)
    {
        NRF_LOG_INFO("nrf_crypto_init() failed");
        APP_ERROR_HANDLER(res);
    }

#endif // NRF_CRYPTO_ENABLED

#if defined(SOFTDEVICE_PRESENT) && SOFTDEVICE_PRESENT

    {
        uint32_t appRAMStart = 0;

        // Configure the BLE stack using the default settings.
        // Fetch the start address of the application RAM.
        res = nrf_sdh_ble_default_cfg_set(BLE_CONN_CONFIG_TAG, &appRAMStart);
        if (res != NRF_SUCCESS)
        {
            NRF_LOG_INFO("nrf_sdh_ble_default_cfg_set() failed");
            APP_ERROR_HANDLER(res);
        }

        // Enable BLE stack.
        res = nrf_sdh_ble_enable(&appRAMStart);
        if (res != NRF_SUCCESS)
        {
            NRF_LOG_INFO("nrf_sdh_ble_enable() failed");
            APP_ERROR_HANDLER(res);
        }
    }

#endif // defined(SOFTDEVICE_PRESENT) && SOFTDEVICE_PRESENT

#if NRF_LOG_ENABLED && NRF_LOG_LEVEL >= NRF_LOG_SEVERITY_INFO

    {
        struct mallinfo minfo = mallinfo();
        NRF_LOG_INFO("System Heap Utilization: heap size %" PRId32 ", arena size %" PRId32 ", in use %" PRId32 ", free %" PRId32,
                GetHeapTotalSize(), minfo.arena, minfo.uordblks, minfo.fordblks);
    }

#endif

    // Create and start a timer to toggle the status LED
    res = app_timer_create(&sStatusLEDTimer, APP_TIMER_MODE_REPEATED, StatusLEDTimerHandler);
    if (res != NRF_SUCCESS)
    {
        NRF_LOG_INFO("app_timer_create() failed");
        APP_ERROR_HANDLER(res);
    }

    // Initialize status and UI LED GPIOs
    nrf_gpio_cfg_output(APP_STATUS_LED_PIN);
    nrf_gpio_pin_set(APP_STATUS_LED_PIN);
    nrf_gpio_cfg_output(APP_UI_LED_PIN);
    nrf_gpio_pin_set(APP_UI_LED_PIN);

    // Initialize the UI button GPIO and the app_button library.
    nrf_gpio_cfg_input(APP_UI_BUTTON_PIN, APP_UI_BUTTON_PULL_CONFIG);
    static app_button_cfg_t sButtonConfigs[] =
    {
        { APP_UI_BUTTON_PIN, APP_UI_BUTTON_ACTIVE_STATE, APP_UI_BUTTON_PULL_CONFIG, ButtonEventHandler }
    };
    res = app_button_init(sButtonConfigs, ARRAY_SIZE(sButtonConfigs), APP_BUTTON_DETECTION_DELAY);
    if (res != NRF_SUCCESS)
    {
        NRF_LOG_INFO("app_button_init() failed");
        APP_ERROR_HANDLER(res);
    }
    res = app_button_enable();
    if (res != NRF_SUCCESS)
    {
        NRF_LOG_INFO("app_button_enable() failed");
        APP_ERROR_HANDLER(res);
    }

#if SOFTDEVICE_PRESENT

    res = BLEPKAPService::Init();
    APP_ERROR_CHECK(res);

#endif // SOFTDEVICE_PRESENT

    NRF_LOG_INFO("Starting main loop");

    while (true)
    {
#if NRF_LOG_DEFERRED
        while (NRF_LOG_PROCESS())
            ;
#endif

#if SOFTDEVICE_PRESENT && NRF_BLE_LESC_ENABLED
        res = nrf_ble_lesc_request_handler();
        APP_ERROR_CHECK(res);
#endif

        nrf_pwr_mgmt_run();
    }
}
