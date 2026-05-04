# Deskbuddy

Deskbuddy turns a low-cost ESP32 touchscreen into a focused desk companion for work: clock, weather icons, hourly and daily forecast, notes, checklist, timer, Wi-Fi setup, brightness, live quotes, technology headlines, daily psalm reading, and a browser-based settings panel.

This fork is tuned for a clean, minimal interface on the CYD 2.8 inch display. The main goal is to make the device useful at a glance during the workday, without needing a phone or laptop tab open.

## Tested Hardware

Tested and working on:

- Board: **ESP32-2432S028R CYD**
- Display: **2.8 inch ILI9341**, 240 x 320
- Touch: **XPT2046**
- Chip detected locally: **ESP32-D0WD-V3**

Known working display configuration is included in [`User_Setup.h`](User_Setup.h):

- Display driver: `ILI9341_2_DRIVER`
- Display SPI bus: `HSPI`
- TFT pins: MISO `12`, MOSI `13`, SCLK `14`, CS `15`, DC `2`
- Backlight pin: `21`
- Touch pins: CS `33`, IRQ `36`, SCK `25`, MISO `39`, MOSI `32`

If the screen stays white, replace the installed TFT_eSPI `User_Setup.h` with the one from this repo.

## Features

- Minimal touch UI with Home, Weather, Notes, Status, and Setup pages
- Redesigned Home page with clock, weather, focus, note, and scrolling live ticker
- Show Mode on Home with rotating large-format slides for clock, weather, focus, checklist, live briefing, and daily psalm
- Apple-inspired Weather page with current conditions, drawn weather icons, hourly forecast, and 7-day forecast
- Focus timer with configurable presets and completion alert
- Notes card for quick daily reminders
- Touch checklist with editable labels and on-device checkboxes
- Live ticker/card with quotes, technology headlines, or mixed rotation with local fallback
- Daily local Bible reflection with psalm/verse reference and short prompt, available without internet
- On-device Setup page for brightness, city presets, live content mode, and Wi-Fi portal
- Local browser settings page after the board joins Wi-Fi
- Open-Meteo weather, hourly forecast, daily forecast, wind, UV, and rain probability
- Sunrise and sunset from sunrise-sunset.org
- KP index from NOAA
- GitHub Actions firmware build
- GitHub Pages installer using ESP Web Tools

## Install From Browser

After the workflow publishes from `main`, open:

```text
https://fernandofatech.github.io/Deskbuddy/
```

Use Chrome or Edge, connect the board by USB, and click **Install Deskbuddy**.

## Arduino Build

Install the Arduino ESP32 core and these libraries:

- `TFT_eSPI`
- `ArduinoJson`
- `XPT2046_Touchscreen`
- `WiFiManager`

Use this board profile:

```text
esp32:esp32:jczn_2432s028r
```

Recommended options:

- Partition scheme: `Huge APP (3MB No OTA/1MB SPIFFS)`
- Upload speed: `115200`

Example compile:

```sh
arduino-cli compile --fqbn esp32:esp32:jczn_2432s028r:UploadSpeed=115200,PartitionScheme=huge_app Deskbuddy
```

Example upload:

```sh
arduino-cli compile --upload -p /dev/cu.usbserial-120 --fqbn esp32:esp32:jczn_2432s028r:UploadSpeed=115200,PartitionScheme=huge_app Deskbuddy
```

## Wi-Fi And Secrets

Do not commit Wi-Fi credentials.

Optional static credentials can be stored locally:

```sh
cp arduino_secrets.example.h arduino_secrets.h
```

Then edit:

```cpp
#define DESKBUDDY_WIFI_SSID "YOUR_WIFI_SSID"
#define DESKBUDDY_WIFI_PASS "YOUR_WIFI_PASSWORD"
```

`arduino_secrets.h` is ignored by Git. If credentials are not present, use the board's Setup page to open the `Deskbuddy Setup` Wi-Fi portal.

Optional AI/API keys must also stay in `arduino_secrets.h`; never commit real keys.

## Documentation

- [Setup Guide](SETUP_GUIDE.md)
- [User Guide](docs/USER_GUIDE.md)
- [Development Guide](docs/DEVELOPMENT.md)

## CI And Pages

The workflow at [`.github/workflows/build-and-pages.yml`](.github/workflows/build-and-pages.yml) compiles the firmware, uploads build artifacts, and deploys the browser installer from `web/` when changes land on `main`.
