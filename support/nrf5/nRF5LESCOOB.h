/*
 *
 *    Copyright (c) Jay Logue
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
 *         Utility functions for working with BLE LESC OOB paring on the
 *         Nordic nRF5 platform.
 */

#ifndef NRF5LESCOOB_H
#define NRF5LESCOOB_H

namespace nrf5utils {

/** Length in bytes of a P-256 public key coordinate
 */
constexpr size_t kP256PubKeyCoordLength = 32;

/** Length in bytes of the BLE LESC OOB confirmation value
 */
constexpr size_t kBLELESCOOBConfirmLength = 16;

/** Length in bytes of the BLE LESC OOB random value
 */
constexpr size_t kBLELESCOOBRandomLength = 16;

ret_code_t ComputeLESCOOBConfirmationValue(const uint8_t * pkx, const uint8_t * r, uint8_t * c);

} // namespace nrf5utils

#endif // NRF5LESCOOB_H
