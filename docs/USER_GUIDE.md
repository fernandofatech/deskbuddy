# Deskbuddy User Guide

Deskbuddy is organized around quick workday checks: time, weather, focus, notes, and device setup.

## Home

Home is the default glance screen. It shows the clock and four configurable widgets.
The refreshed Home screen works like a small smart display. It rotates through large-format slides for at-a-glance status, weather, focus/checklist, live briefing, and a daily psalm reading.

Good default widgets:

- Week number for planning
- Focus timer for work sessions
- Rain for commute or errands
- Outdoor temperature for quick context

Tap the right side of Home to advance a slide, or the left side to go back. The focus slide opens the timer picker and lets you mark checklist rows. Live content is controlled from Setup or the browser settings page. Use `Mix` to rotate between quotes and technology headlines. The daily reading slide is local, so it still works offline after the firmware starts.

## Weather

Weather is designed to answer three questions quickly:

- What is happening now?
- What changes in the next few hours?
- What should I expect this week?

The page shows:

- Current temperature and condition
- Weather icons for clear, cloudy, fog, rain, snow, and storm conditions
- Daily high and low
- Wind and UV
- Next 6 hourly forecast points
- 7-day forecast with condition, high/low, and rain probability

Weather data comes from Open-Meteo.

## Notes

Use Notes for a touch checklist and short reminders that should stay visible during the day, such as:

- Today's priority
- Meeting prep reminder
- A shopping or errand note
- A deployment checklist

Edit checklist labels in the browser settings page, then tap rows on the device to mark them done. The live card below the checklist can show quotes, technology headlines, or a mixed rotation. If public APIs fail, Deskbuddy keeps a local fallback message visible and retries automatically.

## Status

Status shows:

- Wi-Fi state
- Signal strength
- Local IP address
- Uptime
- Last sync time

Use it when debugging network or API updates.

## Setup

Setup is available directly on the touchscreen.

Controls:

- Brightness: tap left or right side of the brightness row
- City: tap left or right side of the city row
- Live card: tap to cycle quote, tech, or off
- Wi-Fi: starts the `Deskbuddy Setup` portal

## Browser Settings

After Wi-Fi connects, open the device IP address in a browser. The web interface lets you configure:

- Notes
- Touch checklist
- Daily psalm view
- Home widgets
- Location and units
- Theme and accent colors
- Timer presets
- Brightness
- Live card mode
- Sleep behavior
