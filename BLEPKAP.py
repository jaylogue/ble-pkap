#
# Copyright (c) 2020 Jay Logue
# All rights reserved.
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#
#

#
#  @file
#        Support code for implementing the BLE-PKAP protocol.
#

import struct
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.asymmetric.utils import encode_dss_signature, decode_dss_signature

BLE_PKAP_SERVICE_UUID   = 'e278ee00-bc2c-e6c0-583c-1c5f15336b24'
BLE_PKAP_AUTH_CHAR_UUID = 'e278ee01-bc2c-e6c0-583c-1c5f15336b24'

class InitiatorAuthToken:
    '''Represents a BLE-PKAP initiator auth token'''

    def __init__(self, sig=None, rand=None, format=None, keyId=0) -> None:
        self.sig = sig
        self.rand = rand
        self.format = format if format is not None else InitiatorAuthToken.FORMAT_V1
        self.keyId = keyId

    def encode(self) -> bytes:
        if self.format != InitiatorAuthToken.FORMAT_V1:
            raise ValueError('Unsupported auth token format')
        if not isinstance(self.rand, bytes) or len(self.rand) != 16:
            raise ValueError('Invalid random value')
        return struct.pack('!BH32s32s16s',
                           self.format, self.keyId,
                           self.sig[0].to_bytes(length=32, byteorder='big'),
                           self.sig[1].to_bytes(length=32, byteorder='big'),
                           self.rand)

    @staticmethod
    def decode(encodedToken) -> 'InitiatorAuthToken':
        format = encodedToken[0]
        if format != InitiatorAuthToken.FORMAT_V1:
            raise ValueError('Unsupported auth token format')
        if len(encodedToken) != 83:
            raise ValueError('Invalid auth token length')
        (format, keyId, sigR, sigS, rand) = struct.unpack('!BH32s32s16s', encodedToken)
        sig = (int.from_bytes(sigR, byteorder='big'),
               int.from_bytes(sigS, byteorder='big'))
        return InitiatorAuthToken(sig=sig, rand=rand, format=format, keyId=keyId)

    def verify(self, confirm, pubKey) -> None:
        if self.format != InitiatorAuthToken.FORMAT_V1:
            raise ValueError('Unsupported auth token format')
        sig = encode_dss_signature(self.sig[0], self.sig[1])
        pubKey.verify(sig, confirm, ec.ECDSA(hashes.SHA256()))

    @staticmethod
    def generate(confirm, rand, keyId, privKey) -> 'InitiatorAuthToken':
        if not isinstance(confirm, bytes) or len(confirm) != 16:
            raise ValueError('Invalid confirmation value')
        if not isinstance(rand, bytes) or len(rand) != 16:
            raise ValueError('Invalid random value')
        sig = privKey.sign(confirm, ec.ECDSA(hashes.SHA256()))
        sig = decode_dss_signature(sig)
        return InitiatorAuthToken(sig=sig, rand=rand, keyId=keyId)

    FORMAT_V1 = 1

class ResponderAuthToken:
    '''Represents a BLE-PKAP responder auth token'''

    def __init__(self, sig=None, format=None, keyId=0) -> None:
        self.sig = sig
        self.format = format if format is not None else ResponderAuthToken.FORMAT_V1
        self.keyId = keyId

    def encode(self) -> bytes:
        if self.format != ResponderAuthToken.FORMAT_V1:
            raise ValueError('Unsupported auth token format')
        return struct.pack('!BH32s32s',
                           self.format, self.keyId,
                           self.sig[0].to_bytes(length=32, byteorder='big'),
                           self.sig[1].to_bytes(length=32, byteorder='big'))

    @staticmethod
    def decode(encodedToken) -> 'ResponderAuthToken':
        format = encodedToken[0]
        if format != ResponderAuthToken.FORMAT_V1:
            raise ValueError('Unsupported auth token format')
        if len(encodedToken) != 67:
            raise ValueError('Invalid auth token length')
        (format, keyId, sigR, sigS) = struct.unpack('!BH32s32s', encodedToken)
        sig = (int.from_bytes(sigR, byteorder='big'),
               int.from_bytes(sigS, byteorder='big'))
        return ResponderAuthToken(sig=sig, format=format, keyId=keyId)

    def verify(self, confirm, pubKey) -> None:
        if self.format != ResponderAuthToken.FORMAT_V1:
            raise ValueError('Unsupported auth token format')
        sig = encode_dss_signature(self.sig[0], self.sig[1])
        pubKey.verify(sig, confirm, ec.ECDSA(hashes.SHA256()))

    @staticmethod
    def generate(confirm, keyId, privKey) -> 'ResponderAuthToken':
        if not isinstance(confirm, bytes) or len(confirm) != 16:
            raise ValueError('Invalid confirmation value')
        sig = privKey.sign(confirm, ec.ECDSA(hashes.SHA256()))
        sig = decode_dss_signature(sig)
        return ResponderAuthToken(sig=sig, keyId=keyId)

    FORMAT_V1 = 1
