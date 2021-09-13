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
 *         Support code for implementing the BLE-PKAP protocol.
 */

#include <sdk_common.h>

#include <inttypes.h>
#include <stdio.h>

#include <ble.h>
#include <nrf_crypto.h>

#if NRF_LOG_ENABLED
#include <nrf_log.h>
#endif // NRF_LOG_ENABLED

#include <BLEPKAP.h>
#include <FunctExitUtils.h>

namespace BLEPKAP {

ret_code_t InitiatorAuthToken::Decode(const uint8_t * buf, size_t len)
{
    ret_code_t res = NRF_SUCCESS;

    // Fail if invalid length or not format version 1
    VerifyOrExit(len > 0, res = NRF_ERROR_DATA_SIZE);
    VerifyOrExit(buf[0] == kFormat_v1, res = NRF_ERROR_NOT_SUPPORTED);
    VerifyOrExit(len == kTokenLen, res = NRF_ERROR_DATA_SIZE);

    // Decode the fields in the token.
    Format = buf[0];
    KeyId = ((uint16_t)buf[1]) << 16 | buf[2];
    Sig = &buf[3];
    Random = Sig + kSigLen;

exit:
    return res;
}

ret_code_t InitiatorAuthToken::Verify(const uint8_t * confirm, size_t confirmLen, const uint8_t * pubKey, size_t pubKeyLen)
{
    ret_code_t res = NRF_SUCCESS;
    uint8_t hashBuf[NRF_CRYPTO_HASH_SIZE_SHA256];
    
    VerifyOrExit(Format != 0, res = NRF_ERROR_INVALID_STATE);
    VerifyOrExit(Format == kFormat_v1, res = NRF_ERROR_NOT_SUPPORTED);
    VerifyOrExit(confirmLen == kConfirmLen, res = NRF_ERROR_INVALID_PARAM);

    {
        nrf_crypto_hash_context_t hashCtx;
        size_t hashLen = sizeof(hashBuf);
        res = nrf_crypto_hash_calculate(&hashCtx, &g_nrf_crypto_hash_sha256_info, 
                                        confirm, confirmLen,
                                        hashBuf, &hashLen);
        SuccessOrExit(res);
    }

    {
        nrf_crypto_ecdsa_verify_context_t verifyCtx;
        nrf_crypto_ecc_public_key_t nrfPubKey;

        res = nrf_crypto_ecc_public_key_from_raw(&g_nrf_crypto_ecc_secp256r1_curve_info,
                                                 &nrfPubKey, pubKey, pubKeyLen);
        SuccessOrExit(res);

        res = nrf_crypto_ecdsa_verify(&verifyCtx, &nrfPubKey, hashBuf, sizeof(hashBuf), Sig, kSigLen);
        SuccessOrExit(res);
    }

exit:
    return res;
}

ret_code_t InitiatorAuthToken::Generate(uint16_t keyId, const uint8_t * confirm, size_t confirmLen,
                                        const uint8_t * random, size_t randomLen,
                                        const uint8_t * privKey, size_t privKeyLen,
                                        uint8_t * outBuf, size_t & outSize)
{
    ret_code_t res = NRF_SUCCESS;
    uint8_t hashBuf[NRF_CRYPTO_HASH_SIZE_SHA256];
    
    VerifyOrExit(outSize >= kTokenLen, res = NRF_ERROR_NO_MEM);
    VerifyOrExit(confirmLen == kConfirmLen, res = NRF_ERROR_INVALID_PARAM);
    VerifyOrExit(randomLen == kRandomLen, res = NRF_ERROR_INVALID_PARAM);

    outBuf[0] = kFormat_v1;
    outBuf[1] = (uint8_t)(keyId >> 8);
    outBuf[2] = (uint8_t)keyId;

    {
        nrf_crypto_hash_context_t hashCtx;
        size_t hashLen = sizeof(hashBuf);
        res = nrf_crypto_hash_calculate(&hashCtx, &g_nrf_crypto_hash_sha256_info, 
                                        confirm, confirmLen,
                                        hashBuf, &hashLen);
        SuccessOrExit(res);
    }

    {
        nrf_crypto_ecdsa_sign_context_t signCtx;
        nrf_crypto_ecc_private_key_t nrfPrivKey;

        res = nrf_crypto_ecc_private_key_from_raw(&g_nrf_crypto_ecc_secp256r1_curve_info,
                                                  &nrfPrivKey, privKey, privKeyLen);
        SuccessOrExit(res);

        size_t sigLen = kSigLen;
        res = nrf_crypto_ecdsa_sign(&signCtx, &nrfPrivKey, hashBuf, sizeof(hashBuf),
                                    outBuf + kHeaderLen, &sigLen);
        nrf_crypto_ecc_private_key_free(&nrfPrivKey);
        SuccessOrExit(res);
        ASSERT(sigLen == kSigLen);
    }

    memcpy(outBuf + kHeaderLen + kSigLen, random, kRandomLen);

    outSize = kTokenLen;

exit:
    return res;
}

ret_code_t ResponderAuthToken::Decode(const uint8_t * buf, size_t len)
{
    ret_code_t res = NRF_SUCCESS;

    // Fail if invalid length or not format version 1
    VerifyOrExit(len > 0, res = NRF_ERROR_DATA_SIZE);
    VerifyOrExit(buf[0] == kFormat_v1, res = NRF_ERROR_NOT_SUPPORTED);
    VerifyOrExit(len == kTokenLen, res = NRF_ERROR_DATA_SIZE);

    // Decode the fields in the token.
    Format = buf[0];
    KeyId = ((uint16_t)buf[1]) << 16 | buf[2];
    Sig = &buf[3];

exit:
    return res;
}

ret_code_t ResponderAuthToken::Verify(const uint8_t * confirm, size_t confirmLen, const uint8_t * pubKey, size_t pubKeyLen)
{
    ret_code_t res = NRF_SUCCESS;
    uint8_t hashBuf[NRF_CRYPTO_HASH_SIZE_SHA256];
    
    VerifyOrExit(Format != 0, res = NRF_ERROR_INVALID_STATE);
    VerifyOrExit(Format == kFormat_v1, res = NRF_ERROR_NOT_SUPPORTED);
    VerifyOrExit(confirmLen == kConfirmLen, res = NRF_ERROR_INVALID_PARAM);

    {
        nrf_crypto_hash_context_t hashCtx;
        size_t hashLen = sizeof(hashBuf);
        res = nrf_crypto_hash_calculate(&hashCtx, &g_nrf_crypto_hash_sha256_info, 
                                        confirm, confirmLen,
                                        hashBuf, &hashLen);
        SuccessOrExit(res);
    }

    {
        nrf_crypto_ecdsa_verify_context_t verifyCtx;
        nrf_crypto_ecc_public_key_t nrfPubKey;

        res = nrf_crypto_ecc_public_key_from_raw(&g_nrf_crypto_ecc_secp256r1_curve_info,
                                                 &nrfPubKey, pubKey, pubKeyLen);
        SuccessOrExit(res);

        res = nrf_crypto_ecdsa_verify(&verifyCtx, &nrfPubKey, hashBuf, sizeof(hashBuf), Sig, kSigLen);
        SuccessOrExit(res);
    }

exit:
    return res;
}

ret_code_t ResponderAuthToken::Generate(uint16_t keyId, const uint8_t * confirm, size_t confirmLen, 
                                        const uint8_t * privKey, size_t privKeyLen,
                                        uint8_t * outBuf, size_t & outSize)
{
    ret_code_t res = NRF_SUCCESS;
    uint8_t hashBuf[NRF_CRYPTO_HASH_SIZE_SHA256];
    
    VerifyOrExit(outSize >= kTokenLen, res = NRF_ERROR_NO_MEM);
    VerifyOrExit(confirmLen == kConfirmLen, res = NRF_ERROR_INVALID_PARAM);

    outBuf[0] = kFormat_v1;
    outBuf[1] = (uint8_t)(keyId >> 8);
    outBuf[2] = (uint8_t)keyId;

    {
        nrf_crypto_hash_context_t hashCtx;
        size_t hashLen = sizeof(hashBuf);
        res = nrf_crypto_hash_calculate(&hashCtx, &g_nrf_crypto_hash_sha256_info, 
                                        confirm, confirmLen,
                                        hashBuf, &hashLen);
        SuccessOrExit(res);
    }

    {
        nrf_crypto_ecdsa_sign_context_t signCtx;
        nrf_crypto_ecc_private_key_t nrfPrivKey;

        res = nrf_crypto_ecc_private_key_from_raw(&g_nrf_crypto_ecc_secp256r1_curve_info,
                                                  &nrfPrivKey, privKey, privKeyLen);
        SuccessOrExit(res);

        size_t sigLen = kSigLen;
        res = nrf_crypto_ecdsa_sign(&signCtx, &nrfPrivKey, hashBuf, sizeof(hashBuf),
                                    outBuf + kHeaderLen, &sigLen);
        nrf_crypto_ecc_private_key_free(&nrfPrivKey);
        SuccessOrExit(res);
        ASSERT(sigLen == kSigLen);
    }

    outSize = kTokenLen;

exit:
    return res;
}

} //namespace BLEPKAP
