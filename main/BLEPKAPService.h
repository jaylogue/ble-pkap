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
 *         A sample BLE service implementing the BLE-PKAP protocol.
 */

#ifndef SAMPLEBLESERVICE_H_
#define SAMPLEBLESERVICE_H_


class SampleBLEService final
{
public:
    static ret_code_t Init(void);
    static void Shutdown(void);
    static void UpdateButtonState(bool isPressed);

private:
    static ret_code_t SetDeviceName(void);
    static ret_code_t ConfigureAdvertising(void);
    static ret_code_t ConfigureGATTService(void);
    static ret_code_t StartAdvertising(void);
    static void HandleBLEEvent(ble_evt_t const * bleEvent, void * context);
    static ret_code_t RegisterVendorUUID(ble_uuid_t & uuid, const ble_uuid128_t & vendorUUID);
    static ble_gap_lesc_oob_data_t *GetPeerLESCOOBData(uint16_t conn_handle);
};

extern void OnAdvertisingStarted(void);
extern void OnConnectionEstablished(void);
extern void OnConnectionTerminated(void);
extern void OnLEDWrite(bool setOn);

#endif // SAMPLEBLESERVICE_H
