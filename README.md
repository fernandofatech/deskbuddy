# Deskbuddy

Deskbuddy is a compact ESP32-based smart desk companion built around a touchscreen display. The project combines 3D printing, simple hardware, and software to turn a raw ESP32 screen into a practical mini dashboard for your workspace.

## Tested Board

This fork has been tested on the **ESP32-2432S028R** 2.8 inch touchscreen board, also commonly sold as:

- **CYD** / Cheap Yellow Display
- **ESP32-2432S028R ILI9341**
- 240 x 320 TFT display with XPT2046 touch

The working display configuration for this board is included in [`User_Setup.h`](User_Setup.h):

- Display driver: `ILI9341_2_DRIVER`
- Display SPI bus: `HSPI`
- TFT pins: MISO `12`, MOSI `13`, SCLK `14`, CS `15`, DC `2`
- Backlight pin: `21`
- Touch pins: CS `33`, IRQ `36`, SCK `25`, MISO `39`, MOSI `32`

If your screen stays white, make sure the included `User_Setup.h` has been copied into the installed `TFT_eSPI` library folder, replacing the library's default setup file.

## Current Features

- Touch dashboard with Home, Weather, Notes, Status, and Setup pages
- On-device Setup page for brightness, city presets, live content mode, and Wi-Fi setup portal
- Browser settings page for notes, theme, widgets, location, timers, brightness, and content mode
- Weather from Open-Meteo
- Sunrise and sunset from sunrise-sunset.org
- KP index from NOAA
- Optional quote card from Quotable
- Optional technology headline from Hacker News Algolia
- GitHub Actions firmware build
- GitHub Pages Web Serial installer

## Wi-Fi Credentials

Do not hardcode Wi-Fi credentials in the sketch.

Copy the example secrets file and edit it locally:

```sh
cp arduino_secrets.example.h arduino_secrets.h
```

Then set:

```cpp
#define DESKBUDDY_WIFI_SSID "YOUR_WIFI_SSID"
#define DESKBUDDY_WIFI_PASS "YOUR_WIFI_PASSWORD"
```

`arduino_secrets.h` is ignored by Git so your network name and password are not committed.

## Arduino Setup

Use the Arduino ESP32 board package and select:

- Board: `ESP32-2432S028R CYD`
- Partition scheme: `Huge APP (3MB No OTA/1MB SPIFFS)`
- Upload speed: `115200` if higher speeds fail

Install these Arduino libraries:

- `TFT_eSPI`
- `ArduinoJson`
- `XPT2046_Touchscreen`
- `WiFiManager`

Before compiling, replace the installed TFT_eSPI setup file with this repo's `User_Setup.h`.

On macOS with `arduino-cli`, the TFT_eSPI setup file is usually:

```sh
~/Documents/Arduino/libraries/TFT_eSPI/User_Setup.h
```

## Notes

The firmware exposes a local web interface after the ESP32 connects to Wi-Fi. Use the serial monitor at `115200` baud to inspect boot logs and find the device IP address.

If Wi-Fi is not configured, open the on-device `Setup` page and tap the Wi-Fi card. Deskbuddy starts a temporary access point named `Deskbuddy Setup`; connect from a phone or computer and choose the desired Wi-Fi network.

## GitHub Pages Installer

The workflow in `.github/workflows/build-and-pages.yml` compiles the firmware and publishes a Web Serial installer from the `web/` folder when changes land on `main`.

The installer expects a generated merged firmware binary at:

```text
firmware/deskbuddy-esp32-2432s028r-cyd.bin
```

GitHub Actions creates that file from the Arduino CLI build output.
## Portfolio / Portfólio

- **Live / Ao vivo:** [Deskbuddy](https://fernando.moretes.com)
- **GitHub:** [fernandofatech/Deskbuddy](https://github.com/fernandofatech/Deskbuddy)
- **Author / Autor:** [Fernando Francisco Azevedo](https://fernando.moretes.com) · [LinkedIn](https://www.linkedin.com/in/fernando-francisco-azevedo/) · [GitHub](https://github.com/fernandofatech)

**PT-BR:** Public fork tracked in Fernando Moretes GitHub portfolio. Este repositório público faz parte do ecossistema de portfólio de Fernando Moretes.

**EN:** Public fork tracked in Fernando Moretes GitHub portfolio. This public repository is part of Fernando Moretes' portfolio ecosystem.
