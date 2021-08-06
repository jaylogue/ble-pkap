# BLE-PKAP Example Application

This project is an example implementation of the BLE Public Key Authenticated Pairing (BLE-PKAP) protocol.
BLE-PKAP supports secure BLE pairing between devices where the devices authenticate each other using
trusted elliptic curve public-private key pairs.  BLE-PKAP builds upon the standard
BLE out-of-band pairing mechanism (LE SC OOB) to provide a mutual authentication scheme
based on public keys.  BLE-PKAP is both more secure and more flexible than traditional BLE
authentication methods (Just-Works, Numeric Comparison, Passkey Entry).  In this way,
BLE-PKAP fills a gap in standard BLE authentication, providing improved security and/or user
experience for applications that need it.

A flow diagram for the BLE-PKAP protocol can be found [here](doc/ble-pkap.png).

<hr>

* [Code Overview](#overview)
* [Installing Prerequisites](#installing-prerequisites)
* [Building the Embedded Application](#building)
* [Initializing the nRF52840 DK](#init-nrf52840)
* [Flashing the Embedded Application](#flash-embedded-app)
* [Viewing Log Output](#view-log-output)
* [Running the Initiator Script](#run-init-script)
* [Example Initiator Output](#example-init-output)
* [Example Responder Output](#example-resp-output)

<hr>


<a name="overview"></a>

## Code Overview

The example code consists of two major components: an **embedded responder application**
that runs on the Nordic nRF52840-DK development board and implements the role of a BLE-PKAP
responder, and a python-based **initiator script** that runs on
Linux with Bluez and implements the BLE-PKAP initiator role.  Together, the examples
demonstrate the use of BLE-PKAP mutual authentication to secure a simple BLE interaction.

<a name="installing-prerequisites"></a>

## Installing Prerequisites

Buiding the embedded application requires installing the Nordic nRF5 SDK and associated
command line tools, a suitable ARM gcc tool chain, and some common build tools, such as
make and ccache.  Additionally, the *SEGGER J-Link Software and Documentation Pack* is
required for debugging the embedded application or viewing its log output.

Running the initiator application requires python3 and installation of the [cryptography](https://cryptography.io/en/latest/index.html) and [PyGObject](https://pygobject.readthedocs.io/en/latest/index.html) python modules.

To install these items perform the following steps:

* Download and install the [Nordic nRF5 SDK version 17.0.2](https://www.nordicsemi.com/Products/Development-software/nrf5-sdk)
([Direct download link](https://www.nordicsemi.com/-/media/Software-and-other-downloads/SDKs/nRF5/Binaries/nRF5SDK1702d674dde.zip))

* Download and install the [Nordic nRF5x Command Line Tools](https://www.nordicsemi.com/Products/Development-tools/nrf-command-line-tools/download)
([Direct download link](https://www.nordicsemi.com/-/media/Software-and-other-downloads/Desktop-software/nRF-command-line-tools/sw/Versions-10-x-x/10-13-0/nRF-Command-Line-Tools_10_13_0_Linux64.zip))

* Download and install the [GNU Arm Embedded Toolchain Version 10.3-2021.07](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads)
([Direct download link](https://developer.arm.com/-/media/Files/downloads/gnu-rm/10.3-2021.07/gcc-arm-none-eabi-10.3-2021.07-x86_64-linux.tar.bz2)) 

* Download the J-Link installer by navigating to the appropriate URL and agreeing to
the license agreement. ([Direct download link](https://www.segger.com/downloads/jlink/JLink_Linux_x86_64.deb))

* Install the J-Link software

        $ cd ~/Downloads
        $ sudo dpkg -i JLink_Linux_V*_x86_64.deb

* In Linux, grant the logged in user the ability to talk to the development hardware
via the linux tty device (/dev/ttyACMx) by adding them to the dialout group.

        $ sudo usermod -a -G dialout ${USER} 

* Install additional build tools:

        $ sudo apt-get install git make ccache netcat-openbsd

* Install python modules:

        $ pip3 install --user cryptography pygobject

<a name="building"></a>

## Building the Embedded Application

The responder embedded application can be built as follows:

* Set the following environment variables based on the locations/versions of the
packages installed above:

        export NRF5_SDK_ROOT=${HOME}/tools/nRF5_SDK_17.0.2_d674dde
        export NRF5_TOOLS_ROOT=${HOME}/tools/nRF-Command-Line-Tools
        export GNU_INSTALL_ROOT=${HOME}/tools/gcc-arm-none-eabi-10.3-2021.07/bin/
        export PATH=${PATH}:${NRF5_TOOLS_ROOT}/nrfjprog

<p style="margin-left: 40px">It is suggested to place these settings in local script file
(e.g. setup-env.sh) such that they can be loaded into the environment as needed, e.g. by
running 'source ./setup-env.sh'.</p>

* Run make in the top-level directory to build the embedded application

        $ cd ~/ble-pkap
        $ make clean
        $ make

<a name="init-nrf52840"></a>

## Initializing the nRF52840 DK

Prior to installing the embedded application for the first time, the flash memory on
the nRF52840-DK board must be erased and the Nordic SoftDevice image installed.

* Connect the host machine to the J-Link Interface MCU USB connector on the nRF52840-DK. (The
Interface MCU connector is the one on the *short* side of the board).

* Use the Makefile to erase the flash memory and program the Nordic SoftDevice image.

        $ make erase
        $ make flash-softdevice

Once the above is complete, it shouldn't need to be done again unless the SoftDevice image becomes
corrupt.  To correct either of these problems erase the device and reflash the SoftDevice and
application again.

<a name="flash-embedded-app"></a>

## Flashing the Embedded Application

To flash the embedded application onto the nRF52840-DK hardware, ensure that the host
machine is connected to the J-Link connector on the nRF52840-DK and run the following command:

        $ make flash-app

This command can be repeated to update the embedded application as changes are made.

<a name="view-log-output"></a>

## Viewing Log Output

The embedded application uses the SEGGER Real Time Transfer (RTT) facility for
log output.  RTT is a feature built-in to the J-Link Interface MCU on the nRF52840-DK
development kit board. It allows bi-directional communication with an embedded application
without the need for a dedicated UART.

Once the embedded application has been flashed, log output from the application can be
viewed using the start-jlink-gdb-server.sh script:

        $ ./start-jlink-gdb-server.sh

It is suggested to run this command in a separate shell window, so the log output can be
viewed while running the initiator script in a main window.

<a name="run-init-script"></a>

## Running the Initiator Script

The BLE-PKAP initiator script can be run from the command-line as follows:

        $ sudo python3 ./ble-pkap-initiator.py <MAC-ADDRESS>

The BLE MAC address of the responding device must be supplied as an argument to the
ble-pkap-initiator.py script.  As a convenience, the embedded responder application
logs its MAC address when it boots.

Upon startup, the initiator application immediately scans and connects to the target
responder device and initiates BLE-PKAP secure pairing.  Once a secure connection has
been established, the user is offered the abilitity to interact with the device by
turning LEDs on and off, and by observing device button presses.

_NOTE: Due to a design limitation in BlueZ, the APIs used by the initiator are
unnecessarily restricted to processes with CAP_NET_ADMIN capability.  Hence the need
for sudo when invoking the initiator script._

<a name="example-init-output"></a>

## Example Initiator Output

```
$ sudo python3 ./ble-pkap-initiator.py C5:9C:EC:6B:88:1A
Scanning for device...
Device found
Initiating BLE connection to device...
BLE connection established
Enumerating device services...
Device services enumerated
BLE-PKAP initiator OOB data:
  Random Value: (16) c1d7e87143f35170b97ab393491ea50c
  Confirmation Value: (16) 83237c9618b19cae650e24a353d949de
BLE-PKAP initiator auth token:
  (83) 0100011af1d80ebb02b98cca1b5030df62d49fe4800a6f4a42d264c4320e963e5b0e18d7f0cb6a417aaddfdb915f8cb041278f9ec2048e4d52feea5284896cc02ac3c2c1d7e87143f35170b97ab393491ea50c
Writing initiator token to PKAP Auth characteristic
Initiating BLE OOB pairing...
BLE OOB pairing SUCCESSFUL
Reading responder token from BLE-PKAP Auth characteristic
BLE-PKAP responder auth token:
  (67) 0100016f225605ce6b673b018611cc0daffbb5376f5d34b15b1c031b941b48adba31b5e8cfa99d19c1bacabf5ce786faf8c4a0f581527b4ff1bc4f114406431374e918
BLE-PKAP responder authentication SUCCESSFUL
BLE-PKAP pairing complete
LED state set to OFF
Ready

Press o to turn LED on
Press f to turn LED off
Press q to quit

LED state set to ON
LED state set to OFF
Button PRESSED
Button RELEASED
Quitting...
Closing BLE connection...
Done
```

<a name="example-resp-output"></a>

## Example Responder Output

```
[00000000] <info> app: ==================================================
[00000000] <info> app: ble-pkap example app starting
[00000000] <info> app: ==================================================
[00000000] <info> app_timer: RTC: initialized.
[00000000] <info> app: Enabling SoftDevice
[00000243] <info> app: Waiting for SoftDevice to be enabled
[00000243] <info> app: SoftDevice enable complete
[00000244] <info> app: System Heap Utilization: heap size 40960, arena size 6360, in use 240, free 6120
[00000244] <debug> nrf_ble_lesc: Initialized nrf_crypto.
[00000244] <debug> nrf_ble_lesc: Initialized nrf_ble_lesc.
[00000244] <debug> nrf_ble_lesc: Generating ECC key pair
[00000263] <info> app: Configuring BLE GATT service
[00000263] <info> app: Adding LED-BUTTON service
[00000264] <info> app: Adding BLE-PKAP service
[00000264] <info> app: Starting BLE advertising (device name: BLE-PKAP-881A, MAC addr: C5:9C:EC:6B:88:1A)
[00000264] <info> app: Starting main loop
[00009348] <debug> nrf_ble_gatt: Requesting to update ATT MTU to 251 bytes on connection 0x0.
[00009348] <debug> nrf_ble_gatt: Updating data length to 251 on connection 0x0.
[00009348] <info> app: BLE connection established (con 0)
[00009530] <debug> nrf_ble_gatt: Data length updated to 27 on connection 0x0.
[00009530] <debug> nrf_ble_gatt: max_rx_octets: 27
[00009530] <debug> nrf_ble_gatt: max_tx_octets: 27
[00009531] <debug> nrf_ble_gatt: max_rx_time: 328
[00009531] <debug> nrf_ble_gatt: max_tx_time: 328
[00009531] <info> app: Other BLE event 36(con 0)
[00009627] <debug> nrf_ble_gatt: ATT MTU updated to 251 bytes on connection 0x0 (response).
[00009628] <info> app: Other BLE event 58(con 0)
[00009676] <debug> nrf_ble_gatt: Peer on connection 0x0 requested an ATT MTU of 517 bytes.
[00009676] <debug> nrf_ble_gatt: Updating ATT MTU to 251 bytes (desired: 251) on connection 0x0.
[00009677] <info> app: Other BLE event 85(con 0)
[00010166] <info> app: BLE-PKAP: Auth characteristic write
[00010166] <info> app: BLE-PKAP: Received peer auth token (len 83)
[00010166] <info> app:     Format: 1
[00010166] <info> app:     KeyId: 1
[00010167] <info> app:     Sig: (64) 1af1d80ebb02b98cca1b5030df62d49fe4800a6f4a42d264c4320e963e5b0e18
[00010167] <info> app:     Random: (16) c1d7e87143f35170b97ab393491ea50c
[00010167] <info> app: BLE-PKAP: Starting BLE-PKAP protocol
[00010261] <info> app: BLE_GAP_EVT_SEC_PARAMS_REQUEST received (con 0)
[00010261] <info> app:     bond: 1
[00010262] <info> app:     mitm: 0
[00010262] <info> app:     lesc: 1
[00010262] <info> app:     keypress: 0
[00010262] <info> app:     io_caps: 0x03
[00010262] <info> app:     oob: 0
[00010262] <info> app:     min_key_size: 0
[00010262] <info> app:     max_key_size: 16
[00010262] <info> app: BLE-PKAP: Starting LESC OOB pairing
[00010263] <info> app:     Local LESC public key:
[00010263] <info> app:         X: (32) 5717de2cee3805dca5c1e99fce087b85fd52a4c9eb2d34081dcf2bf233b9c05a
[00010263] <info> app:         Y: (32) cccafbc20a463f9451bd33d84010fbe4a1fecfb18b6105a99b6f06afae56a657
[00010264] <info> app: sd_ble_gap_sec_params_reply() result (con 0): 0x00000000
[00010360] <info> app: BLE_GAP_EVT_LESC_DHKEY_REQUEST received (con 0)
[00010360] <info> app:     Peer LESC public key:
[00010361] <info> app:         X: (32) d4ca3f7e3fb39ef72ad89afda5b8164060141037bff1d1d8e5ac0af2dca5f916
[00010361] <info> app:         Y: (32) 24518698d20d51935182a82bfa9332acae4d9559ca9f78579ed7ee3a0ef512b4
[00010361] <info> app:     oobd_req: 1
[00010362] <info> app: BLE-PKAP: Peer LESC public key received
[00010362] <debug> nrf_ble_lesc: BLE_GAP_EVT_LESC_DHKEY_REQUEST
[00010362] <info> app: BLE-PKAP: Expected peer OOB confirmation value: (16) 83237c9618b19cae650e24a353d949de
[00010384] <info> app: BLE-PKAP: Peer authentication SUCCESSFUL
[00010405] <info> app: BLE-PKAP: Generated responder auth token (len 67)
[00010405] <info> app:     Format: 1
[00010405] <info> app:     KeyId: 1
[00010405] <info> app:     Sig: (64) 6f225605ce6b673b018611cc0daffbb5376f5d34b15b1c031b941b48adba31b5
[00010425] <info> nrf_ble_lesc: Calling sd_ble_gap_lesc_dhkey_reply on conn_handle: 0
[00010895] <info> app: BLE_GAP_EVT_CONN_SEC_UPDATE received (con 0)
[00010895] <info> app:     sec mode: 0x01
[00010895] <info> app:     sec level: 0x04
[00010895] <info> app:     encr_key_size: 16
[00010896] <info> app: BLE_GAP_EVT_AUTH_STATUS received (con 0)
[00010896] <info> app:     auth_status: 0x00
[00010896] <info> app:     error_src: 0x00
[00010896] <info> app:     bonded: 0
[00010896] <info> app:     lesc: 1
[00010896] <info> app:     sec mode 1, level 1: 1
[00010896] <info> app:     sec mode 1, level 2: 1
[00010897] <info> app:     sec mode 1, level 3: 1
[00010897] <info> app:     sec mode 1, level 4: 1
[00010897] <info> app:     sec mode 2, level 1: 0
[00010897] <info> app:     sec mode 2, level 2: 0
[00010897] <info> app: BLE-PKAP: Pairing complete
[00011041] <info> app: LED characteristic write: OFF
[00039462] <info> app: LED characteristic write: ON
[00040388] <info> app: LED characteristic write: OFF
[00049472] <info> app: Button state change: PRESSED
[00049553] <info> app: Other BLE event 87(con 0)
[00049647] <info> app: Button state change: RELEASED
[00049699] <info> app: Other BLE event 87(con 0)
[00066908] <info> app: BLE connection terminated (con 0, reason 0x13)
[00066908] <info> app: BLE-PKAP: Auth state cleared
[00066909] <debug> nrf_ble_lesc: Generating ECC key pair
[00066927] <info> app: Starting BLE advertising (device name: BLE-PKAP-881A, MAC addr: C5:9C:EC:6B:88:1A)
```