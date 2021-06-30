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
#        Python module for interacting with the Bluez management API.
#

import bluetooth
import struct
from collections import OrderedDict 

HCI_DEV_NONE                                = 0xffff

HCI_CHANNEL_RAW                             = 0
HCI_CHANNEL_USER                            = 1
HCI_CHANNEL_MONITOR                         = 2
HCI_CHANNEL_CONTROL                         = 3
HCI_CHANNEL_LOGGING                         = 4

MGMT_CMD_READ_LOCAL_OOB_DATA                = 0x0020
MGMT_CMD_READ_LOCAL_OOB_EXTENDED_DATA       = 0x003b

MGMT_EV_CMD_COMPLETE                        = 0x0001
MGMT_EV_CMD_STATUS                          = 0x0002
MGMT_EV_CONTROLLER_ERROR                    = 0x0003

MGMT_ADDR_TYPE_BR_EDR                       = 0x01
MGMT_ADDR_TYPE_LE_PUBLIC                    = 0x02
MGMT_ADDR_TYPE_LE_RANDOM                    = 0x04
MGMT_ADDR_TYPE_LE_PUBLIC_AND_RANDOM         = (MGMT_ADDR_TYPE_LE_PUBLIC|MGMT_ADDR_TYPE_LE_RANDOM)

MGMT_STATUS_SUCCESS                         = 0x00
MGMT_STATUS_UNKNOWN_COMMAND                 = 0x01
MGMT_STATUS_NOT_CONNECTED                   = 0x02
MGMT_STATUS_FAILED                          = 0x03
MGMT_STATUS_CONNECT_FAILED                  = 0x04
MGMT_STATUS_AUTH_FAILED                     = 0x05
MGMT_STATUS_NOT_PAIRED                      = 0x06
MGMT_STATUS_NO_RESOURCES                    = 0x07
MGMT_STATUS_TIMEOUT                         = 0x08
MGMT_STATUS_ALREADY_CONNECTED               = 0x09
MGMT_STATUS_BUSY                            = 0x0a
MGMT_STATUS_REJECTED                        = 0x0b
MGMT_STATUS_NOT_SUPPORTED                   = 0x0c
MGMT_STATUS_INVALID_PARAMS                  = 0x0d
MGMT_STATUS_DISCONNECTED                    = 0x0e
MGMT_STATUS_NOT_POWERED                     = 0x0f
MGMT_STATUS_CANCELLED                       = 0x10
MGMT_STATUS_INVALID_INDEX                   = 0x11
MGMT_STATUS_RFKILLED                        = 0x12
MGMT_STATUS_ALREADY_PAIRED                  = 0x13
MGMT_STATUS_PERMISSION_DENIED               = 0x14

BLE_GAP_AD_TYPE_FLAGS                               = 0x01
BLE_GAP_AD_TYPE_16BIT_SERVICE_UUIDS_INCOMPLETE      = 0x02
BLE_GAP_AD_TYPE_16BIT_SERVICE_UUIDS_COMPLETE        = 0x03
BLE_GAP_AD_TYPE_32BIT_SERVICE_UUIDS_INCOMPLETE      = 0x04
BLE_GAP_AD_TYPE_32BIT_SERVICE_UUIDS_COMPLETE        = 0x05
BLE_GAP_AD_TYPE_128BIT_SERVICE_UUIDS_INCOMPLETE     = 0x06
BLE_GAP_AD_TYPE_128BIT_SERVICE_UUIDS_COMPLETE       = 0x07
BLE_GAP_AD_TYPE_SHORTENED_LOCAL_NAME                = 0x08
BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME                 = 0x09
BLE_GAP_AD_TYPE_TX_POWER_LEVEL                      = 0x0A
BLE_GAP_AD_TYPE_CLASS_OF_DEVICE                     = 0x0D
BLE_GAP_AD_TYPE_SIMPLE_PAIRING_HASH_C               = 0x0E
BLE_GAP_AD_TYPE_SIMPLE_PAIRING_RANDOMIZER_R         = 0x0F
BLE_GAP_AD_TYPE_SECURITY_MANAGER_TK_VALUE           = 0x10
BLE_GAP_AD_TYPE_SECURITY_MANAGER_OOB_FLAGS          = 0x11
BLE_GAP_AD_TYPE_SLAVE_CONNECTION_INTERVAL_RANGE     = 0x12
BLE_GAP_AD_TYPE_16BIT_SERVICE_SOLICITAION_UUIDS     = 0x14
BLE_GAP_AD_TYPE_128BIT_SERVICE_SOLICITAION_UUIDS    = 0x15
BLE_GAP_AD_TYPE_SERVICE_DATA                        = 0x16
BLE_GAP_AD_TYPE_PUBLIC_TARGET_ADDRESS               = 0x17
BLE_GAP_AD_TYPE_RANDOM_TARGET_ADDRESS               = 0x18
BLE_GAP_AD_TYPE_APPEARANCE                          = 0x19
BLE_GAP_AD_TYPE_ADVERTISING_INTERVAL                = 0x1A
BLE_GAP_AD_TYPE_LE_BLUETOOTH_DEVICE_ADDRESS         = 0x1B
BLE_GAP_AD_TYPE_LE_ROLE                             = 0x1C
BLE_GAP_AD_TYPE_SIMPLE_PAIRING_HASH_C256            = 0x1D
BLE_GAP_AD_TYPE_SIMPLE_PAIRING_RANDOMIZER_R256      = 0x1E
BLE_GAP_AD_TYPE_32BIT_SERVICE_SOLICITAION_UUIDS     = 0x1F
BLE_GAP_AD_TYPE_SERVICE_DATA_32BIT_UUID             = 0x20
BLE_GAP_AD_TYPE_SERVICE_DATA_128BIT_UUID            = 0x21
BLE_GAP_AD_TYPE_LESC_CONFIRMATION_VALUE             = 0x22
BLE_GAP_AD_TYPE_LESC_RANDOM_VALUE                   = 0x23
BLE_GAP_AD_TYPE_URI                                 = 0x24
BLE_GAP_AD_TYPE_INDOOR_POSITIONING                  = 0x25
BLE_GAP_AD_TYPE_TRANSPORT_DISCOVERY_DATA            = 0x26
BLE_GAP_AD_TYPE_LE_SUPPORTED_FEATURES               = 0x27
BLE_GAP_AD_TYPE_CHANNEL_MAP_UPDATE_INDICATION       = 0x28
BLE_GAP_AD_TYPE_PB_ADV                              = 0x29
BLE_GAP_AD_TYPE_MESH_MESSAGE                        = 0x2A
BLE_GAP_AD_TYPE_MESH_BEACON                         = 0x2B
BLE_GAP_AD_TYPE_BIGINFO                             = 0x2C
BLE_GAP_AD_TYPE_BROADCAST_CODE                      = 0x2D
BLE_GAP_AD_TYPE_3D_INFORMATION_DATA                 = 0x3D
BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA          = 0xFF

def decodeAdvertisingData(advData):
    adElems = OrderedDict()
    i = 0
    while i < len(advData):
        (elemLen, elemType) = struct.unpack_from('<BB', advData, offset=i)
        if i + elemLen + 1 > len(advData):
            raise ValueError('Invalid advertising data element length')
        adElems[elemType] = advData[i+2:i+elemLen+1]
        i += elemLen + 1
    return adElems

class BluezManagementError(Exception):
    def __init__(self, status):
        self.status = status
        self.message = 'BluezManagementError(%d)' % (status)

class BluezManagementSocket(bluetooth.BluetoothSocket):
    
    _maxRecvLen = 512
    
    def __init__(self):
        super(BluezManagementSocket, self).__init__(proto=bluetooth.HCI)
        self.bind((HCI_DEV_NONE, HCI_CHANNEL_CONTROL))

    def issueRequest(self, opCode, controllerIndex, reqData=None):
        if reqData is None:
            reqData = b''
        
        reqHeader = struct.pack('<HHH', opCode, controllerIndex, len(reqData))
        
        self.send(reqHeader + reqData)

        rcvData = b''

        while True:

            rcvData = rcvData + self.recv(BluezManagementSocket._maxRecvLen)
            if len(rcvData) < 6:
                continue

            (eventCode, eventControllerIndex, eventDataLen) = struct.unpack_from('<HHH', rcvData)
            
            if len(rcvData) < eventDataLen + 6:
                continue

            if eventCode == MGMT_EV_CMD_COMPLETE or eventCode == MGMT_EV_CMD_STATUS:
                if len(rcvData) >= 9:
                    (completedOpCode, status) = struct.unpack_from('<HB', rcvData, offset=6)
                    if completedOpCode == opCode:
                        return (status, rcvData[9:eventDataLen+6] if (eventCode == MGMT_EV_CMD_COMPLETE) else None)

            rcvData = rcvData[6 + eventDataLen:]

    def readLocalOOBExtendedData(self, controllerIndex=0, addrType=MGMT_ADDR_TYPE_LE_PUBLIC_AND_RANDOM):
        reqData = struct.pack('<B', addrType)
        (status, data) = self.issueRequest(MGMT_CMD_READ_LOCAL_OOB_EXTENDED_DATA, controllerIndex, reqData)
        if status != MGMT_STATUS_SUCCESS:
            raise BluezManagementError(status)
        if len(data) < 3:
            raise ValueError('Invalid response length for MGMT_CMD_READ_LOCAL_OOB_EXTENDED_DATA command')
        (addrType, dataLen) = struct.unpack_from('<BH', data)
        if len(data) != 3 + dataLen:
            raise ValueError('Invalid response length for MGMT_CMD_READ_LOCAL_OOB_EXTENDED_DATA command')
        oobDataElems = decodeAdvertisingData(data[3:])
        return (addrType, oobDataElems)
