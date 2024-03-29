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
 *         Configuration overrides for the ble-pkap embedded application.
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

// ----- Main Application Config -----

#define APP_STATUS_LED_PIN BSP_LED_0
#define APP_STATUS_LED_BLINK_INTERVAL 500
#define APP_UI_LED_PIN  BSP_LED_1
#define APP_UI_BUTTON_PIN BSP_BUTTON_0
#define APP_UI_BUTTON_ACTIVE_STATE APP_BUTTON_ACTIVE_LOW
#define APP_UI_BUTTON_PULL_CONFIG NRF_GPIO_PIN_PULLUP
#define APP_BUTTON_DETECTION_DELAY APP_TIMER_TICKS(50)

// ----- SimpleBLEApp Class Config -----

#define SIMPLE_BLE_APP_DEFAULT_DEVICE_NAME "BLE-PKAP"
#define SIMPLE_BLE_APP_UNIQUE_DEVICE_NAME 0
#define SIMPLE_BLE_APP_EXTERNAL_PAIRING 1

// ----- LEDButtonService Class Config -----

// Restrict access to LED-Button service characteristics to authenticated
// peer's only (security mode 1, level 4).
#define LED_BUTTON_SERVICE_CHAR_PERM { 1, 4 }

// ----- Memory Config -----

#define MEM_MANAGER_ENABLED 1
#define MEMORY_MANAGER_SMALL_BLOCK_COUNT 4
#define MEMORY_MANAGER_SMALL_BLOCK_SIZE 32
#define MEMORY_MANAGER_MEDIUM_BLOCK_COUNT 4
#define MEMORY_MANAGER_MEDIUM_BLOCK_SIZE 256
#define MEMORY_MANAGER_LARGE_BLOCK_COUNT 1
#define MEMORY_MANAGER_LARGE_BLOCK_SIZE 1024

// ----- Crypto Config -----

#define NRF_CRYPTO_ENABLED 1
#define NRF_CRYPTO_BACKEND_CC310_ENABLED 1
#define NRF_CRYPTO_RNG_AUTO_INIT_ENABLED 1
#define NRF_CRYPTO_ALLOCATOR 3 // 3 = malloc

// ----- SoftDevice Config -----

#define SOFTDEVICE_PRESENT 1
#define S140 1
#define NRF_SDH_ENABLED 1
#define NRF_SDH_SOC_ENABLED 1
#define NRF_SDH_BLE_ENABLED 1
#define NRF_SDH_BLE_PERIPHERAL_LINK_COUNT 1
#define NRF_SDH_BLE_VS_UUID_COUNT 3
#define NRF_BLE_GATT_ENABLED 1
#define NRF_SDH_BLE_GATT_MAX_MTU_SIZE 251
#define NRF_SDH_BLE_GAP_DATA_LENGTH 251
#define BLE_CONN_CONFIG_TAG 1

// ----- FDS / Flash Config -----

#define FDS_ENABLED 1
#define FDS_BACKEND NRF_FSTORAGE_SD
#define NRF_FSTORAGE_ENABLED 1
// Number of virtual flash pages used for FDS data storage.
// NOTE: This value must correspond to FDS_FLASH_PAGES specified in
// linker directives (.ld) file.
#define FDS_VIRTUAL_PAGES 2

// ----- Logging Config -----

#define NRF_LOG_ENABLED 1
#define NRF_LOG_DEFAULT_LEVEL 4
#define NRF_LOG_DEFERRED 0
#define NRF_LOG_NON_DEFFERED_CRITICAL_REGION_ENABLED 1
#define NRF_LOG_STR_PUSH_BUFFER_SIZE 16
#define NRF_LOG_USES_TIMESTAMP 1
#define NRF_LOG_STR_FORMATTER_TIMESTAMP_FORMAT_ENABLED 0

// Select logging back-end
#define NRF_LOG_BACKEND_RTT_ENABLED 1
#define NRF_LOG_BACKEND_UART_ENABLED 0

#if NRF_LOG_BACKEND_UART_ENABLED

#define NRF_LOG_BACKEND_UART_TX_PIN 6
#define NRF_LOG_BACKEND_UART_BAUDRATE 30801920
#define NRF_LOG_BACKEND_UART_TEMP_BUFFER_SIZE 64

#endif // NRF_LOG_BACKEND_UART_ENABLED

#if NRF_LOG_BACKEND_RTT_ENABLED

#define SEGGER_RTT_CONFIG_BUFFER_SIZE_UP 4096
#define SEGGER_RTT_CONFIG_MAX_NUM_UP_BUFFERS 1
#define SEGGER_RTT_CONFIG_BUFFER_SIZE_DOWN 16
#define SEGGER_RTT_CONFIG_MAX_NUM_DOWN_BUFFERS 1
#define SEGGER_RTT_CONFIG_DEFAULT_MODE 1    // 0=SKIP, 1=TRIM, 2=BLOCK_IF_FIFO_FULL

#define NRF_LOG_BACKEND_RTT_TEMP_BUFFER_SIZE 64
#define NRF_LOG_BACKEND_RTT_TX_RETRY_DELAY_MS 1
#define NRF_LOG_BACKEND_RTT_TX_RETRY_CNT 3

#endif // NRF_LOG_BACKEND_RTT_ENABLED

// ----- UART Config -----

#define UART_ENABLED 1
#define UART0_ENABLED 1
#define NRFX_UARTE_ENABLED 1
#define NRFX_UART0_ENABLED 0
#define NRFX_UARTE0_ENABLED 0
#define NRFX_UART_ENABLED 1
#define UART_LEGACY_SUPPORT 1
#define UART_EASY_DMA_SUPPORT 1

// ----- Misc Config -----

// Enable the Nordic ASSERT() macro.  When using FreeRTOS, this also has the
// effect of enabling FreeRTOS configASSERT() due to logic in FreeRTOSConfig.h
#define DEBUG_NRF 1

#define NRF_CLOCK_ENABLED 1
#define NRF_FPRINTF_ENABLED 1
#define NRF_STRERROR_ENABLED 1
#define NRF_QUEUE_ENABLED 1
#define APP_SCHEDULER_ENABLED 1
#define BUTTON_ENABLED 1

#define GPIOTE_ENABLED 1
#define GPIOTE_CONFIG_NUM_OF_LOW_POWER_EVENTS 2

#define NRFX_GPIOTE_ENABLED 1

#define NRFX_UART_ENABLED 1
#define NRFX_UARTE_ENABLED 1
#define UART_ENABLED 1
#define UART0_ENABLED 1

#define NRFX_POWER_ENABLED 0
#define POWER_ENABLED 0
#define NRF_PWR_MGMT_ENABLED 1
#define NRF_PWR_MGMT_CONFIG_FPU_SUPPORT_ENABLED 1

#define APP_TIMER_ENABLED 1
#define APP_TIMER_V2 1
#define APP_TIMER_V2_RTC1_ENABLED 1
#define APP_TIMER_CONFIG_OP_QUEUE_SIZE 10

#define NRFX_PRS_ENABLED 1
#define NRFX_PRS_BOX_4_ENABLED 1

#define NRF_BLE_LESC_ENABLED 1

#endif //APP_CONFIG_H
