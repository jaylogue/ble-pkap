/*
 *    Copyright (c) 2021 Jay Logue
 *    Copyright (c) 2017 Nest Labs, Inc.
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
 *    @file
 *      An implementation of the EAX authenticated encryption mode that uses
 *      AESNI instructions for the underlying block cipher.
 *
 */

#ifndef EAX_AESNI_H_
#define EAX_AESNI_H_

#include <EAX.h>
#include <wmmintrin.h>

/** An implementation of EAX mode based on AES-128 using AESNI instructions.
 */
class EAX_128_AESNI final : public EAX
{
public:
    EAX_128_AESNI(void);
    virtual ~EAX_128_AESNI(void);

protected:
    virtual void AESReset(void);
    virtual void AESSetKey(const uint8_t *key, size_t keyLen);
    virtual void AESEncryptBlock(uint8_t *data);

private:
    enum
    {
        kKeyLength      = 16,
        kBlockLength    = 16,
        kRoundCount     = 10
    };

    __m128i mKey[kRoundCount + 1];
};

/** An implementation of EAX mode based on AES-256 using AESNI instructions.
 */
class EAX_256_AESNI final : public EAX
{
public:
    EAX_256_AESNI(void);
    virtual ~EAX_256_AESNI(void);

protected:
    virtual void AESReset(void);
    virtual void AESSetKey(const uint8_t *key, size_t keyLen);
    virtual void AESEncryptBlock(uint8_t *data);

private:
    enum
    {
        kKeyLength      = 32,
        kBlockLength    = 16,
        kRoundCount     = 14
    };

    __m128i mKey[kRoundCount + 1];
};

#endif // EAX_AESNI_H_
