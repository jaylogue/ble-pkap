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
#        An example implementation of a BLE-PKAP initiator.
#

import argparse
from gi.repository import GLib, GObject
import sys, os, fcntl, termios
import dasbus
import dasbus.identifier
import dasbus.connection
import dasbus.loop
import dasbus.client.proxy
import dasbus.typing
from cryptography.hazmat.primitives.serialization import load_pem_public_key, load_pem_private_key
from cryptography.hazmat.backends import default_backend
import BluezManagement
from BLEPKAP import InitiatorAuthToken, ResponderAuthToken, BLE_PKAP_AUTH_CHAR_UUID

scriptName = os.path.basename(sys.argv[0])

class BLEPKAPInitiator():
    
    BUTTON_CHAR_UUID = '00001524-1212-efde-1523-785feabcd123'
    LED_CHAR_UUID = '00001525-1212-efde-1523-785feabcd123'

    INITIATOR_PRIV_KEY = b'''\
-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIO6Wr7oIFjaQPG6YLtBMWwwJEt7YRHmcvlgvKxoPFudzoAoGCCqGSM49
AwEHoUQDQgAEgSLr4fEu5N6NytlnJ+mbOCb+hU/vWwUkQpBWymjRocbyMTBokbam
QpO8zDEHAvLeReWj27w6WAoUhSNZV5Q1nA==
-----END EC PRIVATE KEY-----
'''
    INITIATOR_KEY_ID = 1

    RESPONDER_PUB_KEY = b'''\
-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEgSLr4fEu5N6NytlnJ+mbOCb+hU/v
WwUkQpBWymjRocbyMTBokbamQpO8zDEHAvLeReWj27w6WAoUhSNZV5Q1nA==
-----END PUBLIC KEY-----
'''
    RESPONDER_KEY_ID = 1

    def __init__(self):
        self.adapterName = None
        self.deviceMAC = None
        self.bus = None
        self.eventLoop = None
        self.inputSource = None
        self.bluezServiceObj = None
        self.adapterObj = None
        self.deviceObj = None
        self.ledChar = None
        self.buttonChar = None
        self.pkapAuthChar = None
        self.expectedRespAuthToken = None
        self.initPrivKey = None
        self.respPubKey = None

    def main(self, args=None):
        try:
            # Parse the command name argument, along with arguments for the command.
            argParser = ArgumentParser(prog=scriptName)
            argParser.add_argument('-a', '--adapter', dest='adapterName', default='hci0', help='Name of the BLE adapter to use (default: hci0)')
            argParser.add_argument('-i', '--init-key', dest='initKeyFile', metavar='<file-name>', default=None, help='PEM file containing initiator private key')
            argParser.add_argument('-r', '--resp-key', dest='respKeyFile', metavar='<file-name>', default=None, help='PEM file containing responder public key')
            argParser.add_argument('deviceMAC', help='BLE MAC address of the target device. This argument is required.')
            args = argParser.parse_args(args=args)
            self.adapterName = args.adapterName
            self.deviceMAC = args.deviceMAC

            self.loadInitPrivKey(args.initKeyFile)
            self.loadRespPubKey(args.respKeyFile)

            self.bus = dasbus.connection.SystemMessageBus()
            self.bluezRootObj = self.bus.get_proxy('org.bluez', '/')
            
            adapterPath = f'/org/bluez/{self.adapterName}'
            if adapterPath not in self.bluezRootObj.GetManagedObjects():
                raise UsageError(f'{scriptName}: BLE adapter {self.adapterName} not found')
            self.adapterObj = self.bus.get_proxy('org.bluez', adapterPath)
            
            self.eventLoop = dasbus.loop.EventLoop()
        
            self.scanForDevice()
        
            self.eventLoop.run()
            
            return 0
        
        except UsageError as ex:
            print('%s' % ex)
            return 1
        
        except KeyboardInterrupt:
            return 1

        finally:
            if self.eventLoop is not None:
                print('Quitting...')
            if self.deviceObj is not None:
                print('Closing BLE connection...')
                self.deviceObj.Disconnect()
            if self.inputSource is not None:
                self.inputSource.shutdown()
            if self.eventLoop is not None:
                self.eventLoop.quit()
                print('Done')
        
    def scanForDevice(self):
        
        def matchTargetDevice(path, interfaces):
            deviceInterfaceProps = interfaces.get('org.bluez.Device1', None)
            if deviceInterfaceProps is None:
                return
            deviceAddr = deviceInterfaceProps.get('Address', None)
            if deviceAddr is None:
                return
            if deviceAddr.unpack() != self.deviceMAC:
                return
    
            print('Device found')
    
            self.adapterObj.StopDiscovery()
            self.bluezRootObj.InterfacesAdded.disconnect()
                
            self.deviceObj = self.bus.get_proxy('org.bluez', path)
            
            self.connectToDevice()

        devicePath = f'/org/bluez/{self.adapterName}/dev_{self.deviceMAC.replace(":", "_")}'
        if devicePath in self.bluezRootObj.GetManagedObjects():
            self.deviceObj = self.bus.get_proxy('org.bluez', devicePath)
            if self.deviceObj.Connected:
                self.deviceObj.Disconnect()
            self.adapterObj.RemoveDevice(devicePath)
            self.deviceObj = None
        
        print("Scanning for device...")
        self.bluezRootObj.InterfacesAdded.connect(matchTargetDevice)
        self.adapterObj.StartDiscovery()

    def connectToDevice(self):

        def onConnectComplete(result):
            try:
                result()
            except:
                print('Connect failed')
                self.eventLoop.quit()
                raise
            
            try:
                print('BLE connection established')
                self.waitServicesResolved()
            except:
                self.eventLoop.quit()
                raise
        
        print("Initiating BLE connection to device...")
        self.deviceObj.Connect(callback=onConnectComplete)

    def waitServicesResolved(self):

        def detectServicesResolved(interface, changedProps, invalidatedProps):
            if interface != 'org.bluez.Device1':
                return 
            val = changedProps.get('ServicesResolved', None)
            if val is None:
                return
            val = val.unpack()
            if val != True:
                return
            
            print('Device services enumerated')

            self.sendInitiatorAuthToken()
        
        try:
            self.deviceObj.PropertiesChanged.connect(detectServicesResolved)
            if not self.deviceObj.ServicesResolved:
                print('Enumerating device services...')
            else:
                self.deviceObj.PropertiesChanged.disconnect()
                self.pairDevice()
        except:
            self.eventLoop.quit()
            raise

    def sendInitiatorAuthToken(self):

        try:
            # Get the dbus object representing the BLE-PKAP Auth characteristic
            devicePath = dasbus.client.proxy.get_object_path(self.deviceObj)
            self.pkapAuthChar = self.getCharacteristicObj(devicePath, BLE_PKAP_AUTH_CHAR_UUID)
            if self.pkapAuthChar is None:
                self.eventLoop.quit()
                raise RuntimeError('Unable to find BLE-PKAP Auth characteristic')

            # Get the local BLE OOB data for the target BLE adapter
            # Note that the adapter will generate new OOB data every time this operation is invoked.
            (self.initConfirm, rand) = self.getBluezLocalOOBData(self.adapterName)
            print('BLE-PKAP initiator OOB data:')
            print('  Random Value: (%d) %s' % (len(rand), rand.hex()))
            print('  Confirmation Value: (%d) %s' % (len(self.initConfirm), self.initConfirm.hex()))

            # Generate and encode a BLE-PKAP initiator auth token from the OOB data and the initiator's
            # private key.
            initAuthToken = InitiatorAuthToken.generate(self.initConfirm, rand, BLEPKAPInitiator.INITIATOR_KEY_ID, self.initPrivKey)
            initAuthToken = initAuthToken.encode()
            print('BLE-PKAP initiator auth token:')
            print('  (%d) %s' % (len(initAuthToken), initAuthToken.hex()))

            # Send the auth token to the responder.        
            print('Writing initiator token to PKAP Auth characteristic')
            self.pkapAuthChar.WriteValue(initAuthToken, [])
            
            # Initiator BLE pairing with the responding device.
            self.pairDevice()
        except:
            self.eventLoop.quit()
            raise

    def pairDevice(self):
    
        def onPairComplete(result):
            try:
                result()
            except:
                print('BLE OOB pairing FAILED')
                self.eventLoop.quit()
                raise
             
            print('BLE OOB pairing SUCCESSFUL')
    
            self.authResponder()
    
        try:
            if not self.deviceObj.Paired:
                print('Initiating BLE OOB pairing...')
                self.deviceObj.Pair(callback=onPairComplete)
            else:
                print('Device already paired')
                self.startUI()
        except:
            self.eventLoop.quit()
            raise

    def authResponder(self):

        try:
            # Retrieve the responder auth token
            print('Reading responder token from BLE-PKAP Auth characteristic')
            respAuthToken = self.pkapAuthChar.ReadValue([])
            respAuthToken = bytes(respAuthToken)
            print('BLE-PKAP responder auth token:')
            print('  (%d) %s' % (len(respAuthToken), respAuthToken.hex()))

            # Decode the responder auth token
            respAuthToken = ResponderAuthToken.decode(respAuthToken)

            # Verify that we have the responder's public key.
            if respAuthToken.keyId != BLEPKAPInitiator.RESPONDER_KEY_ID:
                print('BLE-PKAP responder authentication failed: unknown responder key id: %d' % (respAuthToken.keyId))
                self.eventLoop.quit()
                return

            # Verify the signature in the auth token against the initiator confirmation value
            # and the responder's public key.
            try:
                respAuthToken.verify(self.initConfirm, self.respPubKey)
            except Exception as ex:
                print('BLE-PKAP responder authentication FAILED: %s' % (ex))
                raise

            print('BLE-PKAP responder authentication SUCCESSFUL')
            print('BLE-PKAP pairing complete')

            self.startUI()
        except:
            self.eventLoop.quit()
            raise

    def startUI(self):

        def onButtonNotify(sourceIface, changedProps, removedProps):
            if 'Value' in changedProps:
                value = changedProps['Value']
                value = value[0]
                if value == 0:
                    print('Button RELEASED')
                else:
                    print('Button PRESSED')

        def onInput(obj, ch):
            ch = ch.lower()
            if ch == 'q':
                self.eventLoop.quit()
            elif ch == 'o':
                self.setLED(on=True)
            elif ch == 'f':
                self.setLED(on=False)
        
        try:
            # Get the dbus objects for the LED and button characteristics
            devicePath = dasbus.client.proxy.get_object_path(self.deviceObj)
            self.ledChar = self.getCharacteristicObj(devicePath, BLEPKAPInitiator.LED_CHAR_UUID)
            self.buttonChar = self.getCharacteristicObj(devicePath, BLEPKAPInitiator.BUTTON_CHAR_UUID)

            # Set the initial state of the LED to off
            self.setLED(False)

            # Listen for changes to the button state
            self.buttonChar.PropertiesChanged.connect(onButtonNotify)
            self.buttonChar.StartNotify()

            # Listen for individual key presses
            self.inputSource = RawInputSource()
            self.inputSource.connect('onCharacter', onInput)
    
            print('Ready')
            print('')
            print('Press o to turn LED on')
            print('Press f to turn LED off')
            print('Press q to quit')
            print('')
        except:
            self.eventLoop.quit()
            raise

    def setLED(self, on):
        self.ledChar.WriteValue([ 1 if on else 0 ], [])
        print('LED state set to %s' % ('ON' if on else 'OFF'))
    
    def getCharacteristicObj(self, containerPath, uuid):
        for objPath, objIntfs in self.bluezRootObj.GetManagedObjects().items():
            if objPath.startswith(containerPath):
                intfProps = objIntfs.get('org.bluez.GattCharacteristic1', None)
                if intfProps is not None:
                    if intfProps['UUID'].unpack() == uuid:
                        return self.bus.get_proxy('org.bluez', objPath)
        return None

    def getBluezLocalOOBData(self, adapterName):

        # Create a 'management socket' for accessing the Bluez management interface.
        s = BluezManagement.BluezManagementSocket()

        # Read the extended LESC OOB data from the BLE adapter
        # Note that every time this function is called the BLE adapter generates new
        # OOB data.
        adapterIndex = int(adapterName.replace('hci', ''))
        (addrType, oobDataElems) = s.readLocalOOBExtendedData(controllerIndex=adapterIndex)

        # Extract and return the OOB confirmation and random values
        confirm = oobDataElems[BluezManagement.BLE_GAP_AD_TYPE_LESC_CONFIRMATION_VALUE]
        rand = oobDataElems[BluezManagement.BLE_GAP_AD_TYPE_LESC_RANDOM_VALUE]
        return (confirm, rand)

    def loadInitPrivKey(self, keyFile):
        if keyFile is None:
            privKey = BLEPKAPInitiator.INITIATOR_PRIV_KEY
            keyFile = 'default initiator private key'
        else:
            try:
                with open(keyFile, 'rb') as f:
                    privKey = f.read()
            except Exception as ex:
                raise UsageError('Unable to open %s\n%s' % (keyFile, ex))
        try:
            self.initPrivKey = load_pem_private_key(privKey, None, backend=default_backend())
        except Exception as ex:
                raise UsageError('Unable to load %s\n%s' % (keyFile, ex))

    def loadRespPubKey(self, keyFile):
        if keyFile is None:
            pubKey = BLEPKAPInitiator.RESPONDER_PUB_KEY
            keyFile = 'default responder public key'
        else:
            try:
                with open(keyFile, 'rb') as f:
                    pubKey = f.read()
            except Exception as ex:
                raise UsageError('Unable to open %s\n%s' % (keyFile, ex))
        try:
            self.respPubKey = load_pem_public_key(pubKey, backend=default_backend())
        except Exception as ex:
                raise UsageError('Unable to load %s\n%s' % (keyFile, ex))

class UsageError(Exception):
    pass

class ArgumentParser(argparse.ArgumentParser):
    def error(self, message):
        raise UsageError('{0}: {1}'.format(self.prog, message))

class RawInputSource(GObject.Object):
    __gsignals__ = {
        'onCharacter': (GObject.SIGNAL_RUN_FIRST, None, (str,))
    }
    
    def __init__(self):
        GObject.GObject.__init__(self)
        
        self._stdinFD = sys.stdin.fileno()

        # Put stdin in non-blocking mode
        stdinFlags = fcntl.fcntl(self._stdinFD, fcntl.F_GETFL)
        stdinFlags = stdinFlags | os.O_NONBLOCK
        fcntl.fcntl(self._stdinFD, fcntl.F_SETFL, stdinFlags)

        # Put input terminal in cbreak mode
        (iflag, oflag, cflag, lflag, ispeed, ospeed, cc) = termios.tcgetattr(self._stdinFD)
        self._prevTermios = [iflag, oflag, cflag, lflag, ispeed, ospeed, cc]
        lflag = lflag & ~termios.ICANON
        lflag = lflag & ~termios.ECHO
        cc[termios.VMIN] = 1
        cc[termios.VTIME] = 0
        termios.tcsetattr(self._stdinFD, termios.TCSANOW, [iflag, oflag, cflag, lflag, ispeed, ospeed, cc])
        
        # Monitor stdin for input
        self._sourceId = GLib.io_add_watch(sys.stdin, GLib.IO_IN, self._onDataReady)

    def _onDataReady(self, fd, condition):
        if (condition & GLib.IO_IN) != 0:
            input = fd.read()
            for ch in input:
                self.emit('onCharacter', ch)
        return True
    
    def shutdown(self):
        
        # Remove event source
        GLib.source_remove(self._sourceId)
        
        # Restore previous terminal mode
        (iflag, oflag, cflag, lflag, ispeed, ospeed, cc) = termios.tcgetattr(self._stdinFD)
        termios.tcsetattr(self._stdinFD, termios.TCSANOW, self._prevTermios)

if __name__ == '__main__':
    sys.exit(BLEPKAPInitiator().main())
