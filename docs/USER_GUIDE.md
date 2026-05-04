# Deskbuddy User Guide

Deskbuddy is organized around quick workday checks: time, weather, focus, notes, and device setup.

## Home

Home is the default glance screen. It shows the clock and four configurable widgets.
The refreshed Home screen prioritizes the workday: large time, current weather, focus timer, next-hour weather, a short note, and a scrolling live ticker for quotes or technology headlines.

Good default widgets:

- Week number for planning
- Focus timer for work sessions
- Rain for commute or errands
- Outdoor temperature for quick context

The focus card opens the timer picker. The ticker content is controlled from Setup or the browser settings page. Use `Mix` to rotate between quotes and technology headlines.

## Weather

Weather is designed to answer three questions quickly:

- What is happening now?
- What changes in the next few hours?
- What should I expect this week?

The page shows:

- Current temperature and condition
- Daily high and low
- Wind and UV
- Next 6 hourly forecast points
- 7-day forecast with condition, high/low, and rain probability

Weather data comes from Open-Meteo.

## Notes

Use Notes for short reminders that should stay visible during the day, such as:

- Today's priority
- Meeting prep reminder
- A shopping or errand note
- A deployment checklist

The live card below Notes can show quotes, technology headlines, or a mixed rotation. If public APIs fail, Deskbuddy keeps a local fallback message visible and retries automatically.

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

- Nickname
- Notes
- Home widgets
- Location and units
- Theme and accent colors
- Timer presets
- Brightness
- Live card mode
- Sleep behavior
