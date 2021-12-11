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

#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include <EAX-AESNI.h>

#define ExpandRoundKey128(KEYS, N, RCON, TMP)                           \
do {                                                                    \
    TMP = _mm_aeskeygenassist_si128(KEYS[N-1], RCON);                   \
    TMP = _mm_shuffle_epi32(TMP, 0xff);                                 \
    KEYS[N] = _mm_xor_si128(KEYS[N-1], _mm_slli_si128(KEYS[N-1], 4));   \
    KEYS[N] = _mm_xor_si128(KEYS[N], _mm_slli_si128(KEYS[N], 4));       \
    KEYS[N] = _mm_xor_si128(KEYS[N], _mm_slli_si128(KEYS[N], 4));       \
    KEYS[N] = _mm_xor_si128(KEYS[N], TMP);                              \
} while (0)

EAX_128_AESNI::EAX_128_AESNI(void)
{
    ClearSecretData(&mKey, sizeof(mKey));
}

EAX_128_AESNI::~EAX_128_AESNI(void)
{
    ClearSecretData(&mKey, sizeof(mKey));
}

void EAX_128_AESNI::AESReset(void)
{
    ClearSecretData(&mKey, sizeof(mKey));
}

void EAX_128_AESNI::AESSetKey(const uint8_t *key, size_t keyLen)
{
    __m128i tmp;

    assert(keyLen == kKeyLength);

    mKey[0] = _mm_loadu_si128((__m128i *)key);
    ExpandRoundKey128(mKey, 1, 0x01, tmp);
    ExpandRoundKey128(mKey, 2, 0x02, tmp);
    ExpandRoundKey128(mKey, 3, 0x04, tmp);
    ExpandRoundKey128(mKey, 4, 0x08, tmp);
    ExpandRoundKey128(mKey, 5, 0x10, tmp);
    ExpandRoundKey128(mKey, 6, 0x20, tmp);
    ExpandRoundKey128(mKey, 7, 0x40, tmp);
    ExpandRoundKey128(mKey, 8, 0x80, tmp);
    ExpandRoundKey128(mKey, 9, 0x1b, tmp);
    ExpandRoundKey128(mKey, 10, 0x36, tmp);
    ClearSecretData((uint8_t *)&tmp, sizeof(tmp));
}

void EAX_128_AESNI::AESEncryptBlock(uint8_t *data)
{
    __m128i block;

    block = _mm_loadu_si128((const __m128i *)data);
    block = _mm_xor_si128(block, mKey[0]);
    block = _mm_aesenc_si128(block, mKey[1]);
    block = _mm_aesenc_si128(block, mKey[2]);
    block = _mm_aesenc_si128(block, mKey[3]);
    block = _mm_aesenc_si128(block, mKey[4]);
    block = _mm_aesenc_si128(block, mKey[5]);
    block = _mm_aesenc_si128(block, mKey[6]);
    block = _mm_aesenc_si128(block, mKey[7]);
    block = _mm_aesenc_si128(block, mKey[8]);
    block = _mm_aesenc_si128(block, mKey[9]);
    block = _mm_aesenclast_si128(block, mKey[10]);
    _mm_storeu_si128((__m128i*)data, block);
    ClearSecretData((uint8_t *)&block, sizeof(block));
}

#define ExpandEvenRoundKey256(KEYS, N, RCON, TMP)     \
do {                                                  \
    TMP = _mm_slli_si128(KEYS[N-2], 0x4);             \
    KEYS[N] = _mm_xor_si128(KEYS[N-2], TMP);          \
    TMP = _mm_slli_si128(TMP, 0x4);                   \
    KEYS[N] = _mm_xor_si128(KEYS[N], TMP);            \
    TMP = _mm_slli_si128(TMP, 0x4);                   \
    KEYS[N] = _mm_xor_si128(KEYS[N], TMP);            \
    TMP = _mm_aeskeygenassist_si128(KEYS[N-1], RCON); \
    TMP = _mm_shuffle_epi32(TMP, 0xff);               \
    KEYS[N] = _mm_xor_si128(KEYS[N], TMP);            \
} while (0)
                                                      \
#define ExpandOddRoundKey256(KEYS, N, TMP)            \
do {                                                  \
    TMP = _mm_slli_si128(KEYS[N-2], 0x4);             \
    KEYS[N] = _mm_xor_si128(KEYS[N-2], TMP);          \
    TMP = _mm_slli_si128(TMP, 0x4);                   \
    KEYS[N] = _mm_xor_si128(KEYS[N], TMP);            \
    TMP = _mm_slli_si128(TMP, 0x4);                   \
    KEYS[N] = _mm_xor_si128(KEYS[N], TMP);            \
    TMP = _mm_aeskeygenassist_si128(KEYS[N-1], 0x0);  \
    TMP = _mm_shuffle_epi32(TMP, 0xaa);               \
    KEYS[N] = _mm_xor_si128 (KEYS[N], TMP);           \
} while (0)

EAX_256_AESNI::EAX_256_AESNI(void)
{
    ClearSecretData(&mKey, sizeof(mKey));
}

EAX_256_AESNI::~EAX_256_AESNI(void)
{
    ClearSecretData(&mKey, sizeof(mKey));
}

void EAX_256_AESNI::AESReset(void)
{
    ClearSecretData(&mKey, sizeof(mKey));
}

void EAX_256_AESNI::AESSetKey(const uint8_t *key, size_t keyLen)
{
    __m128i tmp;

    assert(keyLen == kKeyLength);

    mKey[0] = _mm_loadu_si128((const __m128i *)key);
    mKey[1] = _mm_loadu_si128((const __m128i *)(key + 16));
    ExpandEvenRoundKey256(mKey, 2, 0x01, tmp);
    ExpandOddRoundKey256(mKey, 3, tmp);
    ExpandEvenRoundKey256(mKey, 4, 0x02, tmp);
    ExpandOddRoundKey256(mKey, 5, tmp);
    ExpandEvenRoundKey256(mKey, 6, 0x04, tmp);
    ExpandOddRoundKey256(mKey, 7, tmp);
    ExpandEvenRoundKey256(mKey, 8, 0x08, tmp);
    ExpandOddRoundKey256(mKey, 9, tmp);
    ExpandEvenRoundKey256(mKey, 10, 0x10, tmp);
    ExpandOddRoundKey256(mKey, 11, tmp);
    ExpandEvenRoundKey256(mKey, 12, 0x20, tmp);
    ExpandOddRoundKey256(mKey, 13, tmp);
    ExpandEvenRoundKey256(mKey, 14, 0x40, tmp);
    ClearSecretData((uint8_t *)&tmp, sizeof(tmp));
}

void EAX_256_AESNI::AESEncryptBlock(uint8_t *data)
{
    __m128i block;

    block = _mm_loadu_si128((const __m128i *)data);
    block = _mm_xor_si128(block, mKey[0]);
    block = _mm_aesenc_si128(block, mKey[1]);
    block = _mm_aesenc_si128(block, mKey[2]);
    block = _mm_aesenc_si128(block, mKey[3]);
    block = _mm_aesenc_si128(block, mKey[4]);
    block = _mm_aesenc_si128(block, mKey[5]);
    block = _mm_aesenc_si128(block, mKey[6]);
    block = _mm_aesenc_si128(block, mKey[7]);
    block = _mm_aesenc_si128(block, mKey[8]);
    block = _mm_aesenc_si128(block, mKey[9]);
    block = _mm_aesenc_si128(block, mKey[10]);
    block = _mm_aesenc_si128(block, mKey[11]);
    block = _mm_aesenc_si128(block, mKey[12]);
    block = _mm_aesenc_si128(block, mKey[13]);
    block = _mm_aesenclast_si128(block, mKey[14]);
    _mm_storeu_si128((__m128i*)data, block);
    ClearSecretData((uint8_t *)&block, sizeof(block));
}

//
// Compile as follows to create a stand-alone program for testing EAX-128-AESNI
// against the standard test vectors.
//
//    c++ -O3 -maes -o test-eax-128-aesni -I. -DUNIT_TEST EAX.cpp EAX-AESNI.cpp EAXTest.cpp
//
#ifdef UNIT_TEST

#include <stdio.h>

int main(int argc, char *argv[])
{
    EAX_128_AESNI eax;
    TestEAX128(eax);
    printf("All tests passed\n");
}

#endif // UNIT_TEST
