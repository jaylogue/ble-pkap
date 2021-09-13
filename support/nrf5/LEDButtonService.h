/**
 * Copyright (c) 2021 Jay Logue
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
 *         An implementation of the Nordic LED-Button BLE service for use with the Nordic
 *         SoftDevice and nRF5 SDK.
 */

#ifndef LEDBUTTONSERVICE_H_
#define LEDBUTTONSERVICE_H_

namespace nrf5utils {

/** Implements the core logic for the Nordic LED-Button BLE service.
 */
class LEDButtonService final
{
public:
    static ret_code_t Init(void);
    static void UpdateButtonState(bool isPressed);
    static void ButtonEventHandler(uint8_t buttonPin, uint8_t buttonAction);
    static void GetServiceUUID(ble_uuid_t & serviceUUID);

    struct Event final
    {
        static void OnLEDWrite(bool setOn) __WEAK;
    };

private:
    static void HandleBLEEvent(ble_evt_t const * bleEvent, void * context);
};



/** Compile-time configuration options for the LEDButtonService class
 * @{
 */

/** Observer priority for LEDButtonService module
 */
#ifndef LED_BUTTON_SERVICE_OBSERVER_PRIO
#define LED_BUTTON_SERVICE_OBSERVER_PRIO 3
#endif // LED_BUTTON_SERVICE_OBSERVER_PRIO

/** Access control permission for LEDButtonService characteristics
 * 
 * Expected value is a structure initialization expression for a structure containing
 * two integers: the security mode and the security level.
 * 
 * Possible values:
 *      { 0, 0 } - No access
 *      { 1, 1 } - No security
 *      { 1, 2 } - Unauthenticated pairing with encryption
 *      { 1, 3 } - Authenticated pairing with encryption
 *      { 1, 4 } - Authenticated LESC pairing with encryption
 */
#ifndef LED_BUTTON_SERVICE_CHAR_PERM 
#define LED_BUTTON_SERVICE_CHAR_PERM { 1, 1 }
#endif // LED_BUTTON_SERVICE_CHAR_PERM

/** @} */

} // namespace nrf5utils

#endif // LEDBUTTONSERVICE_H_
