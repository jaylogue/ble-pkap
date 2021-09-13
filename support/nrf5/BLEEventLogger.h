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
 *         Support for automatic logging of Nordic SoftDevice BLE events.
 */

#ifndef BLEEVENTLOGGER_H_
#define BLEEVENTLOGGER_H_

namespace nrf5utils {

class BLEEventLogger final
{
public:
    static ret_code_t Init(void);

private:
    static void HandleBLEEvent(ble_evt_t const * bleEvent, void * context);
};

} // namespace nrf5utils


/** Compile-time configuration options for the BLEEventLogger class
 * @{ */

/** Observer priority for BLEEventLogger module
 */
#ifndef BLE_EVENT_LOGGER_OBSERVER_PRIO
#define BLE_EVENT_LOGGER_OBSERVER_PRIO 0
#endif // BLE_EVENT_LOGGER_OBSERVER_PRIO

/**@} */

#endif // BLEEVENTLOGGER_H_