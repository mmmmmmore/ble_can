# ESP32-S3 BLE to TWAI Transparent Bridge

This ESP-IDF project creates a simple BLE peripheral on the ESP32-S3. When an iPad or another BLE central writes bytes to the RX characteristic, the firmware forwards the same payload over CAN using the ESP32 `twai` driver.

## Project layout

- `main/` - `app_main()` entry point
- `platform/init/` - startup/shutdown state management for all resources
- `components/bluetooth/` - BLE/NimBLE basic initialization and RX callback registration
- `components/twai/` - TWAI GPIO setup and CAN driver start/stop
- `app/bluetooth_rx/` - BLE RX handling and retransmission trigger
- `app/twai_tx/` - chunk BLE payload into 8-byte CAN frames and transmit

## Default hardware settings

- **Target**: `esp32s3`
- **TWAI TX GPIO**: `GPIO5`
- **TWAI RX GPIO**: `GPIO4`
- **Default CAN ID**: `0x321`
- **Bitrate**: `500 kbit/s`
- **BLE device name**: `ESP32S3-BLE-CAN`
- **BLE service UUID**: Nordic UART style `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
- **BLE RX characteristic UUID**: `6E400002-B5A3-F393-E0A9-E50E24DCCA9E`

> A **CAN transceiver** is required between the ESP32-S3 and the CAN bus.

## Build and flash

```bash
cd /Users/maochun/Gitprj/esp32/ble_can
source /Users/maochun/Gitprj/Tools/esp-idf/esp-idf/export.sh
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/tty.usbmodemXXXX flash monitor
```

## Basic test flow

1. Connect the board to a TWAI/CAN transceiver.
2. Flash the firmware.
3. Use an iPad BLE app such as LightBlue to connect to `ESP32S3-BLE-CAN`.
4. Write bytes to the RX characteristic.
5. Observe matching TWAI frames on the CAN bus analyzer.

Large BLE writes are split transparently into 8-byte TWAI frames in send order.


function test scan can found the device, next step for pad and CAN test
