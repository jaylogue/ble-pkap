#
#
#   Copyright (c) 2020 Jay Logue
#   All rights reserved.
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#

# 
#   @file
#         Makefile for building the BLE-PKAP example application.
#

PROJECT_ROOT := $(realpath .)

include $(PROJECT_ROOT)/nrf5-app.mk

APP = ble-pkap-responder-app

SRCS = \
    $(PROJECT_ROOT)/main/main.cpp \
    $(PROJECT_ROOT)/main/BLEPKAPService.cpp \
    $(PROJECT_ROOT)/support/nrf5/nRF5SysTime.cpp \
    $(PROJECT_ROOT)/support/nrf5/nRF5LESCOOB.cpp \
    $(PROJECT_ROOT)/support/general/CXXExceptionStubs.cpp \
    $(PROJECT_ROOT)/support/nrf5/nRF5Sbrk.c \
    $(PROJECT_ROOT)/support/general/AltPrintf.c \
    $(PROJECT_ROOT)/support/nrf5/AltNRFPrintf.c \
    $(PROJECT_ROOT)/support/general/JLinkMMDStubs.c \
    $(PROJECT_ROOT)/external/printf/printf.c \
    $(NRF5_SDK_ROOT)/components/ble/common/ble_advdata.c \
    $(NRF5_SDK_ROOT)/components/ble/common/ble_conn_state.c \
    $(NRF5_SDK_ROOT)/components/ble/common/ble_srv_common.c \
    $(NRF5_SDK_ROOT)/components/ble/nrf_ble_gatt/nrf_ble_gatt.c \
    $(NRF5_SDK_ROOT)/components/ble/peer_manager/nrf_ble_lesc.c \
    $(NRF5_SDK_ROOT)/components/boards/boards.c \
    $(NRF5_SDK_ROOT)/components/libraries/atomic/nrf_atomic.c \
    $(NRF5_SDK_ROOT)/components/libraries/atomic_fifo/nrf_atfifo.c \
    $(NRF5_SDK_ROOT)/components/libraries/atomic_flags/nrf_atflags.c \
    $(NRF5_SDK_ROOT)/components/libraries/balloc/nrf_balloc.c \
    $(NRF5_SDK_ROOT)/components/libraries/button/app_button.c \
    $(NRF5_SDK_ROOT)/components/libraries/crc16/crc16.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/backend/nrf_hw/nrf_hw_backend_init.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/backend/nrf_hw/nrf_hw_backend_rng.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/backend/nrf_hw/nrf_hw_backend_rng_mbedtls.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/backend/cc310/cc310_backend_aes.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/backend/cc310/cc310_backend_aes_aead.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/backend/cc310/cc310_backend_chacha_poly_aead.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/backend/cc310/cc310_backend_ecc.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/backend/cc310/cc310_backend_ecdh.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/backend/cc310/cc310_backend_ecdsa.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/backend/cc310/cc310_backend_eddsa.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/backend/cc310/cc310_backend_hash.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/backend/cc310/cc310_backend_hmac.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/backend/cc310/cc310_backend_init.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/backend/cc310/cc310_backend_mutex.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/backend/cc310/cc310_backend_rng.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/backend/cc310/cc310_backend_shared.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/nrf_crypto_aead.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/nrf_crypto_aes.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/nrf_crypto_aes_shared.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/nrf_crypto_ecc.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/nrf_crypto_ecdh.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/nrf_crypto_ecdsa.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/nrf_crypto_eddsa.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/nrf_crypto_error.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/nrf_crypto_hash.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/nrf_crypto_hkdf.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/nrf_crypto_hmac.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/nrf_crypto_init.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/nrf_crypto_rng.c \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/nrf_crypto_shared.c \
    $(NRF5_SDK_ROOT)/components/libraries/experimental_section_vars/nrf_section_iter.c \
    $(NRF5_SDK_ROOT)/components/libraries/hardfault/hardfault_implementation.c \
    $(NRF5_SDK_ROOT)/components/libraries/fds/fds.c \
    $(NRF5_SDK_ROOT)/components/libraries/fstorage/nrf_fstorage.c \
    $(NRF5_SDK_ROOT)/components/libraries/fstorage/nrf_fstorage_sd.c \
    $(NRF5_SDK_ROOT)/components/libraries/log/src/nrf_log_backend_rtt.c \
    $(NRF5_SDK_ROOT)/components/libraries/log/src/nrf_log_backend_serial.c \
    $(NRF5_SDK_ROOT)/components/libraries/log/src/nrf_log_backend_uart.c \
    $(NRF5_SDK_ROOT)/components/libraries/log/src/nrf_log_default_backends.c \
    $(NRF5_SDK_ROOT)/components/libraries/log/src/nrf_log_frontend.c \
    $(NRF5_SDK_ROOT)/components/libraries/log/src/nrf_log_str_formatter.c \
    $(NRF5_SDK_ROOT)/components/libraries/mem_manager/mem_manager.c \
    $(NRF5_SDK_ROOT)/components/libraries/memobj/nrf_memobj.c \
    $(NRF5_SDK_ROOT)/components/libraries/pwr_mgmt/nrf_pwr_mgmt.c \
    $(NRF5_SDK_ROOT)/components/libraries/queue/nrf_queue.c \
    $(NRF5_SDK_ROOT)/components/libraries/ringbuf/nrf_ringbuf.c \
    $(NRF5_SDK_ROOT)/components/libraries/scheduler/app_scheduler.c \
    $(NRF5_SDK_ROOT)/components/libraries/sortlist/nrf_sortlist.c \
    $(NRF5_SDK_ROOT)/components/libraries/strerror/nrf_strerror.c \
    $(NRF5_SDK_ROOT)/components/libraries/timer/app_timer2.c \
    $(NRF5_SDK_ROOT)/components/libraries/timer/drv_rtc.c \
    $(NRF5_SDK_ROOT)/components/libraries/util/app_error.c \
    $(NRF5_SDK_ROOT)/components/libraries/util/app_error_handler_gcc.c \
    $(NRF5_SDK_ROOT)/components/libraries/util/app_error_weak.c \
    $(NRF5_SDK_ROOT)/components/libraries/util/app_util_platform.c \
    $(NRF5_SDK_ROOT)/components/libraries/util/nrf_assert.c \
    $(NRF5_SDK_ROOT)/components/softdevice/common/nrf_sdh.c \
    $(NRF5_SDK_ROOT)/components/softdevice/common/nrf_sdh_ble.c \
    $(NRF5_SDK_ROOT)/components/softdevice/common/nrf_sdh_soc.c \
    $(NRF5_SDK_ROOT)/external/segger_rtt/SEGGER_RTT.c \
    $(NRF5_SDK_ROOT)/external/segger_rtt/SEGGER_RTT_Syscalls_GCC.c \
    $(NRF5_SDK_ROOT)/external/segger_rtt/SEGGER_RTT_printf.c \
    $(NRF5_SDK_ROOT)/integration/nrfx/legacy/nrf_drv_clock.c \
    $(NRF5_SDK_ROOT)/integration/nrfx/legacy/nrf_drv_uart.c \
    $(NRF5_SDK_ROOT)/modules/nrfx/drivers/src/nrfx_clock.c \
    $(NRF5_SDK_ROOT)/modules/nrfx/drivers/src/nrfx_gpiote.c \
    $(NRF5_SDK_ROOT)/modules/nrfx/drivers/src/nrfx_uart.c \
    $(NRF5_SDK_ROOT)/modules/nrfx/drivers/src/nrfx_uarte.c \
    $(NRF5_SDK_ROOT)/modules/nrfx/drivers/src/prs/nrfx_prs.c \
    $(NRF5_SDK_ROOT)/modules/nrfx/mdk/gcc_startup_nrf52840.S \
    $(NRF5_SDK_ROOT)/modules/nrfx/mdk/system_nrf52840.c \
    $(NRF5_SDK_ROOT)/modules/nrfx/soc/nrfx_atomic.c

INC_DIRS = \
    $(PROJECT_ROOT)/main \
    $(PROJECT_ROOT)/support/general \
    $(PROJECT_ROOT)/support/nrf5 \
    $(PROJECT_ROOT)/external/printf \
    $(NRF5_SDK_ROOT)/components \
    $(NRF5_SDK_ROOT)/components/boards \
    $(NRF5_SDK_ROOT)/components/ble/ble_advertising \
    $(NRF5_SDK_ROOT)/components/ble/common \
    $(NRF5_SDK_ROOT)/components/ble/nrf_ble_gatt \
    $(NRF5_SDK_ROOT)/components/ble/peer_manager \
    $(NRF5_SDK_ROOT)/components/libraries/atomic \
    $(NRF5_SDK_ROOT)/components/libraries/atomic_flags \
    $(NRF5_SDK_ROOT)/components/libraries/atomic_fifo \
    $(NRF5_SDK_ROOT)/components/libraries/balloc \
    $(NRF5_SDK_ROOT)/components/libraries/bsp \
    $(NRF5_SDK_ROOT)/components/libraries/button \
    $(NRF5_SDK_ROOT)/components/libraries/crc16 \
    $(NRF5_SDK_ROOT)/components/libraries/crypto \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/backend/cc310 \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/backend/cc310_bl \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/backend/cifra \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/backend/mbedtls \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/backend/micro_ecc \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/backend/nrf_hw \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/backend/nrf_sw \
    $(NRF5_SDK_ROOT)/components/libraries/crypto/backend/oberon \
	$(NRF5_SDK_ROOT)/components/libraries/crypto/backend/optiga \
    $(NRF5_SDK_ROOT)/components/libraries/delay \
    $(NRF5_SDK_ROOT)/components/libraries/experimental_section_vars \
    $(NRF5_SDK_ROOT)/components/libraries/fds \
    $(NRF5_SDK_ROOT)/components/libraries/fstorage \
    $(NRF5_SDK_ROOT)/components/libraries/log \
    $(NRF5_SDK_ROOT)/components/libraries/log/src \
    $(NRF5_SDK_ROOT)/components/libraries/memobj \
    $(NRF5_SDK_ROOT)/components/libraries/mem_manager \
    $(NRF5_SDK_ROOT)/components/libraries/mutex \
    $(NRF5_SDK_ROOT)/components/libraries/queue \
    $(NRF5_SDK_ROOT)/components/libraries/ringbuf \
    $(NRF5_SDK_ROOT)/components/libraries/sortlist \
    $(NRF5_SDK_ROOT)/components/libraries/pwr_mgmt \
    $(NRF5_SDK_ROOT)/components/libraries/stack_info \
    $(NRF5_SDK_ROOT)/components/libraries/strerror \
    $(NRF5_SDK_ROOT)/components/libraries/timer \
    $(NRF5_SDK_ROOT)/components/libraries/util \
    $(NRF5_SDK_ROOT)/components/softdevice/common \
    $(NRF5_SDK_ROOT)/components/softdevice/s140/headers \
    $(NRF5_SDK_ROOT)/components/softdevice/s140/headers/nrf52 \
    $(NRF5_SDK_ROOT)/components/softdevice/mbr/nrf52840/headers \
    $(NRF5_SDK_ROOT)/components/toolchain/cmsis/include \
    $(NRF5_SDK_ROOT)/config/nrf52840/config \
    $(NRF5_SDK_ROOT)/external/fprintf \
    $(NRF5_SDK_ROOT)/external/nrf_cc310/include \
    $(NRF5_SDK_ROOT)/external/nrf_tls/mbedtls/nrf_crypto/config \
    $(NRF5_SDK_ROOT)/external/segger_rtt \
    $(NRF5_SDK_ROOT)/integration/nrfx \
    $(NRF5_SDK_ROOT)/integration/nrfx/legacy \
    $(NRF5_SDK_ROOT)/modules/nrfx \
    $(NRF5_SDK_ROOT)/modules/nrfx/drivers/include \
    $(NRF5_SDK_ROOT)/modules/nrfx/hal \
    $(NRF5_SDK_ROOT)/modules/nrfx/mdk

LIBS = \
    $(NRF5_SDK_ROOT)/external/nrf_cc310/lib/cortex-m4/hard-float/libnrf_cc310_0.9.13.a

DEFINES = \
    NRF52840_XXAA \
    BOARD_PCA10056 \
    CONFIG_GPIO_AS_PINRESET \
    FLOAT_ABI_HARD \
    USE_APP_CONFIG \
    __HEAP_SIZE=40960 \
    __STACK_SIZE=8192 \
    SOFTDEVICE_PRESENT \
    PRINTF_DISABLE_SUPPORT_EXPONENTIAL


CFLAGS = \
    --specs=nano.specs -Wno-unused-function

LDFLAGS = \
    --specs=nano.specs

LINKER_SCRIPT = $(PROJECT_ROOT)/ldscripts/nrf52-baseline-app-with-softdevice.ld

$(call GenerateBuildRules)
