# Deskbuddy Setup Guide

This guide covers the tested setup for the ESP32-2432S028R CYD board.

## 1. Hardware

Use a USB data cable and connect the board to the computer. The tested board is:

- ESP32-2432S028R CYD
- ILI9341 240 x 320 TFT display
- XPT2046 resistive touch

If flashing fails, try another cable before changing code.

## 2. Install Arduino Tools

Install:

- Arduino IDE or `arduino-cli`
- ESP32 board package by Espressif
- `TFT_eSPI`
- `ArduinoJson`
- `XPT2046_Touchscreen`
- `WiFiManager`

Board profile:

```text
ESP32-2432S028R CYD
```

CLI FQBN:

```text
esp32:esp32:jczn_2432s028r
```

## 3. Configure TFT_eSPI

Replace the installed TFT_eSPI `User_Setup.h` with this repo's [`User_Setup.h`](User_Setup.h).

Typical macOS path:

```sh
~/Documents/Arduino/libraries/TFT_eSPI/User_Setup.h
```

This is the key fix for white-screen issues.

## 4. Wi-Fi Setup

Recommended path:

1. Flash the firmware.
2. Open the `Setup` page on the device.
3. Tap the Wi-Fi card.
4. Connect your phone or computer to `Deskbuddy Setup`.
5. Choose your Wi-Fi network in the portal.

Optional developer path:

```sh
cp arduino_secrets.example.h arduino_secrets.h
```

Edit `arduino_secrets.h` locally. It is ignored by Git.

## 5. Compile And Upload

Compile:

```sh
arduino-cli compile --fqbn esp32:esp32:jczn_2432s028r:UploadSpeed=115200,PartitionScheme=huge_app Deskbuddy
```

Upload:

```sh
arduino-cli compile --upload -p /dev/cu.usbserial-120 --fqbn esp32:esp32:jczn_2432s028r:UploadSpeed=115200,PartitionScheme=huge_app Deskbuddy
```

If upload does not start automatically, hold BOOT while the upload begins.

## 6. First Boot

The device should:

- Turn on the display
- Connect to Wi-Fi or wait for setup
- Sync time
- Fetch weather and live content
- Start the local browser settings page

Use Serial Monitor at `115200` baud to see the local IP address.

## Troubleshooting

White screen:

- Replace TFT_eSPI `User_Setup.h`
- Confirm `ILI9341_2_DRIVER`
- Confirm board profile `jczn_2432s028r`

Touch wrong or inverted:

- Confirm XPT2046 pins in `User_Setup.h`
- Confirm rotation in `desk_buddy_github.cpp`

Wi-Fi not connecting:

- Use the on-device `Setup` page and start the Wi-Fi portal
- Confirm the network is 2.4 GHz
- Check serial logs

Weather not updating:

- Confirm Wi-Fi is online
- Confirm city/lat/lng in Setup or browser settings
- Check that public APIs are reachable from your network

