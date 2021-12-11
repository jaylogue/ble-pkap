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
 *         An implemenation of EAX authenticated encryption mode for
 *         use on the Nordic nRF5 platforms.
 */

#ifndef NRF5EAX_H_
#define NRF5EAX_H_

#if NRF_CRYPTO_ENABLED
#include <nrf_crypto.h>
#endif // NRF_CRYPTO_ENABLED

#include <EAX.h>

#if NRF_CRYPTO_ENABLED

/** Base class for EAX-AES mode implemented using the Nordic nrf_crypto library.
 */
class EAX_nrfcrypto_base : public EAX
{
protected:
    EAX_nrfcrypto_base(const nrf_crypto_aes_info_t * aesInfo);
    virtual ~EAX_nrfcrypto_base(void);
    virtual void AESReset(void);
    virtual void AESSetKey(const uint8_t * key, size_t keyLen);
    virtual void AESEncryptBlock(uint8_t * data);

private:
    const nrf_crypto_aes_info_t * mAESInfo;
    nrf_crypto_aes_context_t mAESCtx;
};

/** Implementation of EAX mode for AES-128 using the Nordic nrf_crypto library.
 */
class EAX_128_nrfcrypto final : public EAX_nrfcrypto_base
{
public:
    EAX_128_nrfcrypto(void);
    virtual ~EAX_128_nrfcrypto(void);
};

/** Implementation of EAX mode for AES-256 using the Nordic nrf_crypto library.
 */
class EAX_256_nrfcrypto final : public EAX_nrfcrypto_base
{
public:
    EAX_256_nrfcrypto(void);
    virtual ~EAX_256_nrfcrypto(void);
};

#endif // NRF_CRYPTO_ENABLED

#if defined(SOFTDEVICE_PRESENT) && SOFTDEVICE_PRESENT

/** Implementation of EAX mode for AES-128 using the Nordic SoftDevice API.
 * 
 *  This class uses the sd_ecb_block_encrypt() function as the underlying
 *  block cipher.
 */

class EAX_128_SD final : public EAX
{
public:
    EAX_128_SD();
    virtual ~EAX_128_SD(void);

protected:
    virtual void AESReset(void);
    virtual void AESSetKey(const uint8_t * key, size_t keyLen);
    virtual void AESEncryptBlock(uint8_t * data);

private:
    enum {
        kKeyLength = 16
    };
    uint8_t mKey[kKeyLength];
};

#endif // defined(SOFTDEVICE_PRESENT) && SOFTDEVICE_PRESENT

//
// NOTE: To test the nRF5 EAX classes, add the following code to the example
// application initialization code located in main.cpp:
//
//     #include <nRF5EAX.h>
//
//     ...
//
//     int main(void)
//     {
//         ...
//
//         NRF_LOG_INFO("Testing EAX mode");
//         {
//             EAX_128_nrfcrypto eax;
//             TestEAX128(eax);
//         }
//         {
//             EAX_128_SD eax;
//             TestEAX128(eax);
//         }
//         NRF_LOG_INFO("All tests complete");
//
//         ...
//

#endif // NRF5EAX_H_
