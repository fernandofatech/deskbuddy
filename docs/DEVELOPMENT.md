# Deskbuddy Development Guide

## Project Layout

- `Deskbuddy/Deskbuddy.ino`: Arduino sketch wrapper
- `desk_buddy_github.cpp`: main firmware
- `User_Setup.h`: tested TFT_eSPI display configuration
- `arduino_secrets.example.h`: safe Wi-Fi credential template
- `web/`: GitHub Pages installer
- `.github/workflows/build-and-pages.yml`: CI build and Pages deploy

## Firmware Architecture

The firmware keeps one main loop and a small set of pages:

- `PAGE_HOME`
- `PAGE_WEATHER`
- `PAGE_NOTES`
- `PAGE_STATUS`
- `PAGE_SETTINGS`

Data fetches are throttled and cached:

- Weather: every 10 minutes
- KP index: every 10 minutes
- Live quote/news: every 60 minutes

The UI avoids continuous full-screen redraws. Pages keep cache strings and only redraw changed sections unless `pageDirty` or `dataDirty` is set.

## Public APIs

Deskbuddy currently uses:

- Open-Meteo forecast API
- sunrise-sunset.org
- NOAA planetary K index
- Quotable random quote API
- Hacker News Algolia search API

No API keys are required.

## Build Locally

```sh
arduino-cli compile --fqbn esp32:esp32:jczn_2432s028r:UploadSpeed=115200,PartitionScheme=huge_app Deskbuddy
```

## Upload Locally

```sh
arduino-cli compile --upload -p /dev/cu.usbserial-120 --fqbn esp32:esp32:jczn_2432s028r:UploadSpeed=115200,PartitionScheme=huge_app Deskbuddy
```

## Release Flow

1. Create a feature branch.
2. Compile locally.
3. Upload to a real board when changing display or touch behavior.
4. Push the branch to the fork.
5. Let GitHub Actions build the firmware artifact.
6. Merge to `main` to publish the Web Serial installer.

## Secret Safety

Never commit:

- `arduino_secrets.h`
- real SSIDs
- real Wi-Fi passwords

Run this before committing if credentials were used locally:

```sh
rg -n "YOUR_REAL_WIFI_OR_PASSWORD" -S .
```

