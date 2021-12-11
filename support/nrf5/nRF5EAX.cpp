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
 *         An implemenation of EAX authenticated encryption mode that uses
 *         the Nordic nrf_crypto library for the core AES function.
 */

#include <sdk_common.h>

#include <inttypes.h>
#include <stdio.h>
#include <assert.h>

#if NRF_CRYPTO_ENABLED
#include <nrf_crypto.h>
#endif // NRF_CRYPTO_ENABLED

#include <nrf_soc.h>

#include <nRF5EAX.h>

#if NRF_CRYPTO_ENABLED

EAX_nrfcrypto_base::EAX_nrfcrypto_base(const nrf_crypto_aes_info_t * aesInfo)
{
    ret_code_t res;

    mAESInfo = aesInfo;

    res = nrf_crypto_aes_init(&mAESCtx, aesInfo, NRF_CRYPTO_ENCRYPT);
    assert(res == NRF_SUCCESS);
}

EAX_nrfcrypto_base::~EAX_nrfcrypto_base()
{
    nrf_crypto_aes_uninit(&mAESCtx);
}

void EAX_nrfcrypto_base::AESReset(void)
{
    ret_code_t res;

    nrf_crypto_aes_uninit(&mAESCtx);

    res = nrf_crypto_aes_init(&mAESCtx, mAESInfo, NRF_CRYPTO_ENCRYPT);
    assert(res == NRF_SUCCESS);
}

void EAX_nrfcrypto_base::AESSetKey(const uint8_t *key, size_t keyLen)
{
    ret_code_t res;

    assert(keyLen == (mAESInfo->key_size / 8));

    res = nrf_crypto_aes_key_set(&mAESCtx, (uint8_t *)key);
    assert(res == NRF_SUCCESS);
}

void EAX_nrfcrypto_base::AESEncryptBlock(uint8_t * data)
{
    ret_code_t res;
    
    res = nrf_crypto_aes_update(&mAESCtx, data, NRF_CRYPTO_AES_BLOCK_SIZE, data);
    assert(res == NRF_SUCCESS);
}

EAX_128_nrfcrypto::EAX_128_nrfcrypto(void)
: EAX_nrfcrypto_base(&g_nrf_crypto_aes_ecb_128_info)
{
}

EAX_128_nrfcrypto::~EAX_128_nrfcrypto(void)
{
}

EAX_256_nrfcrypto::EAX_256_nrfcrypto(void)
: EAX_nrfcrypto_base(&g_nrf_crypto_aes_ecb_256_info)
{
}

EAX_256_nrfcrypto::~EAX_256_nrfcrypto(void)
{
}

#endif // NRF_CRYPTO_ENABLED

#if defined(SOFTDEVICE_PRESENT) && SOFTDEVICE_PRESENT

EAX_128_SD::EAX_128_SD(void)
{
}

EAX_128_SD::~EAX_128_SD()
{
    ClearSecretData(mKey, sizeof(mKey));
}

void EAX_128_SD::AESReset(void)
{
    ClearSecretData(mKey, sizeof(mKey));
}

void EAX_128_SD::AESSetKey(const uint8_t *key, size_t keyLen)
{
    assert(keyLen == kKeyLength);
    memcpy(mKey, key, kKeyLength);
}

void EAX_128_SD::AESEncryptBlock(uint8_t * data)
{
    ret_code_t res;
    nrf_ecb_hal_data_t ecbData;

    memcpy(ecbData.key, mKey, SOC_ECB_CLEARTEXT_LENGTH);
    memcpy(ecbData.cleartext, data, SOC_ECB_CLEARTEXT_LENGTH);

    res = sd_ecb_block_encrypt(&ecbData);
    assert(res == NRF_SUCCESS);

    memcpy(data, ecbData.ciphertext, SOC_ECB_CIPHERTEXT_LENGTH);

    ClearSecretData(&ecbData, sizeof(ecbData));
}

#endif // defined(SOFTDEVICE_PRESENT) && SOFTDEVICE_PRESENT
