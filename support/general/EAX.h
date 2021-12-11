/*
 *    Copyright (c) 2021 Jay Logue
 *    Copyright (c) 2013-2017 Nest Labs, Inc.
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
 *      A generic implementation of the EAX authenticated encryption mode.
 *
 */

#ifndef EAX_H_
#define EAX_H_

/* CONFIGURATION OPTIONS */

/** CONFIG_EAX_NO_PAD_CACHE
 * 
 * If non-zero, then the internal "L1" value is not cached; this saves 16
 * bytes of RAM in the EAX class, but requires three extra AES block
 * invocations per message.
 */
#ifndef CONFIG_EAX_NO_PAD_CACHE
#define CONFIG_EAX_NO_PAD_CACHE 0
#endif

/** CONFIG_EAX_NO_CHUNK
 * 
 * If non-zero, then the EAX class expects non-chunked input of header
 * and payload: only a single InjectHeader() call, and a single
 * Encrypt() or Decrypt() call, may be used for a given message. This
 * constrains usage, but saves 32 bytes of RAM in the EAX class. There
 * is no CPU cost penalty.
 */
#ifndef CONFIG_EAX_NO_CHUNK
#define CONFIG_EAX_NO_CHUNK 0
#endif

class EAX;

/** Contains saved message header processing state
 * 
 * An instance of EAXSaved can be filled with intermediate processing results,
 * so that several messages that use the same header and are encrypted or decrypted
 * with the same key can share some of the computational cost.
 * 
 * See EAX::SaveHeader() and EAX::StartSaved() for further details.
 */
class EAXSaved
{
public:
    EAXSaved(void);
    ~EAXSaved(void);
private:
    enum {
        kBlockLength = 16
    };
    uint8_t aad[kBlockLength];  // Saved OMAC^1(header)
#if !CONFIG_EAX_NO_CHUNK
    uint8_t om2[kBlockLength];  // Saved encryption of the OMAC^2 start block
#endif
    friend class EAX;
    static inline void ClearSecretData(void * buf, size_t len) { memset(buf, 0, len); }
};

/** Abstract implementation of EAX block cipher mode
 * 
 * Abstract base class implementing the EAX block cipher mode.  Users
 * must subclass the class 
 *
 * API usage:
 *
 *  - Use SetKey() to set the AES key. This must be done first. 
 *
 *  - Call Start() to start processing a new message. This method must
 *    follow a call to SetKey(). Nonce data is supplied as an optional
 *    argument to this call.
 *
 *  - Process header data with one or several calls to InjectHeader().
 *    This must follow a Start(), but must precede payload encryption
 *    or decryption. If InjectHeader() is not called, a zero-length
 *    header is assumed.
 *
 *  - Encrypt or decrypt the data, with one or several calls to
 *    Encrypt() or Decrypt(). Calls for a given message must be all
 *    encrypt or all decrypt. Encryption and decryption do not change
 *    the length of the data; chunks of arbitrary lengths can be used
 *    (even zero-length chunks).
 *
 *  - Finalize the computation of the authentication tag, and get it
 *    (with GetTag()) or check it (with CheckTag()). Encryption will
 *    typically use GetTag() (to obtain the tag value to send to the
 *    recipient) while decryption more naturally involves calling
 *    CheckTag() (to verify the tag value received from the sender).
 * 
 *  - Call Reset() to reset the internal encryption/decryption state
 *    and clear any secret data.  This may be called at any time.
 *    After a call to Reset(), the object may be reused for a subsequent
 *    encryption/decryption process by calling SetKey().
 * 
 *  - Destroying the object (via invocating of its distructor)
 *    automatically resets the internal state and clears any secret data.
 */
class EAX
{
public:
    enum {
        kMinTagLength = 1,   // minimum tag length, in bytes
        kMaxTagLength = 16   // maximum tag length, in bytes
    };

    /** Reset object
     * 
     * Clear this object from all secret key and data.
     * 
     * After a call to Reset(), the object may be reused for a subsequent
     * encryption/decryption process.
     */
    void Reset(void);

    /** Set encryption key
     * 
     * Set the AES key. The key size depends on the chosen concrete class.
     */
    void SetKey(const uint8_t *key, size_t keyLen);

    /** Process header data for reuse
     * 
     * Process header data and fill the provided 'sav' object with the
     * result. That object can then be reused with StartSaved()
     * to process messages that share the same header value (and
     * use the same key).
     */
    void SaveHeader(const uint8_t *header, size_t headerLen, EAXSaved *sav);

    /** Start encrypting/decrypting a message
     * 
     * Start encrypting/decrypting a new message processing, with the given
     * nonce. This must be immediately preceeded by a call to SetKey().
     * After message processing is complete, Start() may be called again
     * after a call to Reset() and SetKey().
     * 
     * Nonce length is arbitrary, but the same nonce value MUST
     * NOT be reused with the same key for a different message.
     */
    void Start(const uint8_t *nonce, size_t nonceLen);

    /** Start encrypting/decrypting a message using saved header data.
     * 
     * Start encrypting/decrypting a new message processing, with the given
     * nonce and previously processed and saved header data (created via the
     * SaveHeader() method).
     * 
     * The 'sav' object is not modified, and may be reused for other messages
     * that use the same key and header.
     */
    void StartSaved(const uint8_t *nonce, size_t nonceLen, const EAXSaved *sav);

    /** Process header data
     *
     * Process the given header data. The header data is not encrypted,
     * but participates to the authentication tag. Header processing must
     * occur after the call to Start(), but before processing the payload.
     * If no header data is given, then a zero-length header is used.
     *
     * Unless CONFIG_EAX_NO_CHUNK is enabled, the header may be
     * processed in several chunks, via several calls to InjectHeader() with
     * arbitrary chunk lengths.
     */
    void InjectHeader(const uint8_t *header, size_t headerLen);

    /** Encrypt message data
     *
     * Encrypt the provided payload. Input data (plaintext) is read from
     * 'input' and has size 'inputLen' bytes; the corresponding data has
     * the same length and is written in 'output'. The 'input' and 'output'
     * buffers may overlap partially or totally.
     *
     * Unless CONFIG_EAX_NO_CHUNK is enabled, the payload may be
     * processed in several chunks (several calls to Encrypt() with
     * arbitrary chunk lengths).
     */
    void Encrypt(const uint8_t *input, size_t inputLen, uint8_t *output);

    /** Encrypt message data in-place
     *
     * Variant of Encrypt() for in-place processing: the encrypted data
     * replaces the plaintext data in the 'data' buffer.
     */
    void Encrypt(uint8_t *data, size_t dataLen);

    /** Decrypt message data
     *
     * Identical to Encrypt(), except for decryption instead of encryption.
     * Note that, for a given message, all chunks must be encrypted, or
     * all chunks must be decrypted; mixing encryption and decryption for
     * a single message is not permitted.
     */
    void Decrypt(const uint8_t *input, size_t inputLen, uint8_t *output);

    /** Decrypt message data in-place
     *
     * Variant of Decrypt() for in-place processing: the decrypted data
     * replaces the ciphertext data in the 'data' buffer.
     */
    void Decrypt(uint8_t *data, size_t dataLen);

    /** Finalize encryption/decryption
     * 
     * Finalize encryption or decryption, and get the authentication tag.
     * This may be called only once per message; after the tag has been
     * obtained, only SetKey() and Start() may be called again on the
     * instance.
     *
     * Tag length must be between 8 and 16 bytes. Normal EAX tag length is
     * 16 bytes.
     */
    void GetTag(uint8_t *tag, size_t tagLen);

    /** Finalize decryption and check tag
     * 
     * Variant of GetTag() that does not return the tag, but compares it
     * with the provided tag value. This is meant to be used by recipient,
     * to verify the tag on an incoming message. Returned value is true
     * if the tags match, false otherwise. Comparison is constant-time.
     */
    bool CheckTag(const uint8_t *tag, size_t tagLen);

protected:
    
    /** Initialize the object and prepare it for use.
     */
    EAX(void);

    /** Destroy the object and clear all state and secret data.
     * 
     * IMPORTANT: Subclasses are *required* to override the destructor
     * and clear any state or secrets associated with the underlying AES
     * block encryptor.
     */
    virtual ~EAX(void);

    /** Reset the underlying AES block encryptor
     * 
     * ** THIS IS A PURE-VIRTUAL FUNCTION TO BE IMPLEMENTED BY THE SUBCLASS **
     * 
     * This method should reset the underlying AES block encryptor and clear any
     * paload or secret data it may contain, including the encryption key.
     * 
     * AESReset() may be called at any time.  After a call to AESReset(), AESSetKey()
     * may called again to set a different key.
     */
    virtual void AESReset(void) = 0;

    /** Set the encryption key.
     * 
     * ** THIS IS A PURE-VIRTUAL FUNCTION TO BE IMPLEMENTED BY THE SUBCLASS **
     * 
     * AESSetKey() will be called exactly once before any calls to AESEncryptBlock().
     * After a call to AESReset(), AESSetKey() may be called to set a different key.
     * 
     * The implementation must verify the length of the key and fail (assert) if
     * the length is incorrect.
     */
    virtual void AESSetKey(const uint8_t *key, size_t keyLen) = 0;

    /** AES Encrypt a block
     * 
     * ** THIS IS A PURE-VIRTUAL FUNCTION TO BE IMPLEMENTED BY THE SUBCLASS **
     * 
     * Encrypt the 16-byte block pointed at by data and overwrite its contects with
     * the cyphertext.  AESEncryptBlock() may be called any number of times after a
     * call to AESSetKey().
     */
    virtual void AESEncryptBlock(uint8_t *data) = 0;

    static inline void ClearSecretData(void * buf, size_t len) { memset(buf, 0, len); }

private:
    enum {
        kBlockLength = 16
    };

#if !CONFIG_EAX_NO_PAD_CACHE
    uint8_t L1[kBlockLength];
#endif
#if !CONFIG_EAX_NO_CHUNK
    uint8_t buf[kBlockLength];
    uint8_t cbcmac[kBlockLength];
#endif
    uint8_t ctr[kBlockLength];
    uint8_t acc[kBlockLength];
#if !CONFIG_EAX_NO_CHUNK
    uint8_t ptr;
#endif
    uint8_t state;

    void double_gf128(uint8_t *elt);
    void xor_block(const uint8_t *src, uint8_t *dst);
    void omac(unsigned val, const uint8_t *data, size_t len, uint8_t *mac);
#if !CONFIG_EAX_NO_CHUNK
    void omac_start(unsigned val);
    void omac_process(const uint8_t *data, size_t len);
    void omac_finish(unsigned val);
    void aad_finish(void);
#endif
    void incr_ctr(void);
    void payload_process(bool encrypt, uint8_t *data, size_t len);
    void ClearState(void);
};

/** Test a concrete implementation of the EAX class for AES-128 using
 *  standardized test vectors.
 * 
 *  The function will assert() on error.
 */
extern void TestEAX128(EAX & eax);

#endif /* AES_H_ */
