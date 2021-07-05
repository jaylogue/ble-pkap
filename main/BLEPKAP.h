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

#ifndef BLEPKAP_H
#define BLEPKAP_H


namespace BLEPKAP {

class InitiatorAuthToken
{
public:
    static constexpr uint8_t kFormat_v1 = 1;

    static constexpr size_t kHeaderLen = 3;
    static constexpr size_t kSigLen = 64;
    static constexpr size_t kRandomLen = 16;
    static constexpr size_t kTokenLen = kHeaderLen + kSigLen + kRandomLen;

    static constexpr size_t kConfirmLen = 16;

    uint8_t Format;
    uint16_t KeyId;
    const uint8_t * Sig;
    const uint8_t * Random;

    ret_code_t Decode(const uint8_t * buf, size_t len);
    ret_code_t Verify(const uint8_t * confirm, size_t confirmLen, const uint8_t * pubKey, size_t pubKeyLen);
    void Clear();

    static ret_code_t Generate(uint16_t keyId, const uint8_t * confirm, size_t confirmLen, 
                               const uint8_t * random, size_t randomLen,
                               const uint8_t * privKey, size_t privKeyLen,
                               uint8_t * outBuf, size_t & outSize);
};

class ResponderAuthToken
{
public:
    static constexpr uint8_t kFormat_v1 = 1;

    static constexpr size_t kHeaderLen = 3;
    static constexpr size_t kSigLen = 64;
    static constexpr size_t kTokenLen = kHeaderLen + kSigLen;

    static constexpr size_t kConfirmLen = 16;

    uint8_t Format;
    uint16_t KeyId;
    const uint8_t * Sig;

    ret_code_t Decode(const uint8_t * buf, size_t len);
    ret_code_t Verify(const uint8_t * confirm, size_t confirmLen, const uint8_t * pubKey, size_t pubKeyLen);
    void Clear();

    static ret_code_t Generate(uint16_t keyId, const uint8_t * confirm, size_t confirmLen, 
                               const uint8_t * privKey, size_t privKeyLen,
                               uint8_t * outBuf, size_t & outSize);
};

extern const ble_uuid128_t kBLEPKAPServiceUUID128;
extern const ble_uuid128_t kBLEPKAPAuthCharUUID128;

inline void InitiatorAuthToken::Clear()
{
    *this = {};
}

inline void ResponderAuthToken::Clear()
{
    *this = {};
}


} //namespace BLEPKAP

#endif // BLEPKAP_H