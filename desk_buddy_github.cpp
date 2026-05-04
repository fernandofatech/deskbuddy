// Deskbuddy V.8
// Nav: Home / Weather / Notes / Status
// Full version
// - KP dots replaced with Low / Medium / High / Extreme text
// - KP level text uses same small font as wind direction and stays inside the box
// - Wind + direction added to Weather page
// - Wind direction uses Accent color
// - Weather sun event field automatically shows Sunrise or Sunset, whichever is next
// - Uptime added to Status page

#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <TFT_eSPI.h>
#include <time.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <Preferences.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <math.h>

// =========================================================
// WIFI
// =========================================================
#if __has_include("arduino_secrets.h")
#include "arduino_secrets.h"
#endif

#ifndef DESKBUDDY_WIFI_SSID
#define DESKBUDDY_WIFI_SSID "YOUR_WIFI_SSID"
#endif

#ifndef DESKBUDDY_WIFI_PASS
#define DESKBUDDY_WIFI_PASS "YOUR_WIFI_PASSWORD"
#endif

const char* WIFI_SSID = DESKBUDDY_WIFI_SSID;
const char* WIFI_PASS = DESKBUDDY_WIFI_PASS;

static bool hasStaticWifiCredentials() {
  return String(WIFI_SSID) != "YOUR_WIFI_SSID" &&
         String(WIFI_PASS) != "YOUR_WIFI_PASSWORD" &&
         String(WIFI_SSID).length() > 0;
}

// =========================================================
// DISPLAY / TOUCH
// =========================================================
TFT_eSPI tft;

const int ROT = 2;
const bool INV = false;

#define TOUCH_CS  33
#define TOUCH_IRQ 36

static const int T_SCK  = 25;
static const int T_MISO = 39;
static const int T_MOSI = 32;

SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS);

static const int TOUCH_X_MIN = 562;
static const int TOUCH_X_MAX = 3604;
static const int TOUCH_Y_MIN = 544;
static const int TOUCH_Y_MAX = 3720;

static const bool TOUCH_SWAP_XY = false;
static const bool TOUCH_FLIP_X  = false;
static const bool TOUCH_FLIP_Y  = false;

// =========================================================
// WEB / STORAGE
// =========================================================
WebServer server(80);
Preferences prefs;

// =========================================================
// SPRITES
// =========================================================
TFT_eSprite sprClock = TFT_eSprite(&tft);
TFT_eSprite sprSmall = TFT_eSprite(&tft);

// =========================================================
// LOCATION
// =========================================================
float LAT = 52.5200f;
float LNG = 13.4050f;
String locationName = "Berlin";

// =========================================================
// THEME
// =========================================================
uint16_t COL_BG        = 0x08A3;
uint16_t COL_PANEL     = 0x1106;
uint16_t COL_PANEL_ALT = 0x18C7;
uint16_t COL_STROKE    = 0x31EC;
uint16_t COL_TEXT      = 0xEF7D;
uint16_t COL_DIM       = 0x94B2;
uint16_t COL_ACCENT    = 0x5EFA;

const uint16_t COL_GREEN  = TFT_GREEN;
const uint16_t COL_YELLOW = 0xFFE0;
const uint16_t COL_RED    = TFT_RED;
const uint16_t COL_BLUE   = 0x041F;

String textColorKey = "standard";
String unitKey = "metric"; // metric = C/mm, imperial = F/in
String regionFormatKey = "europe"; // europe = 24h + dd.mm.yyyy, us = 12h + mm/dd/yyyy

// =========================================================
// LAYOUT
// =========================================================
const int SCREEN_W = 240;
const int SCREEN_H = 320;
const int TOPBAR_H = 34;
const int NAV_H    = 44;

const int HOME_GRID_Y1 = 120;
const int HOME_GRID_Y2 = 198;
const int HOME_WIDGET_H = 70;

const int HOME_TIMER_X = 124;
const int HOME_TIMER_Y = HOME_GRID_Y1;
const int HOME_TIMER_W = 108;
const int HOME_TIMER_H = HOME_WIDGET_H;
const int TIMER_MENU_X = 20;
const int TIMER_MENU_Y = 68;
const int TIMER_MENU_W = 200;
const int TIMER_MENU_H = 194;
const int TIMER_DONE_X = 26;
const int TIMER_DONE_Y = 92;
const int TIMER_DONE_W = 188;
const int TIMER_DONE_H = 108;
const int PAGE_ROW1_Y = 42;
const int PAGE_ROW2_Y = 120;
const int PAGE_ROW3_Y = 198;
const int PAGE_WIDGET_H = HOME_WIDGET_H;

// =========================================================
// NOTES
// =========================================================
String notesText = "No notes yet.";
bool notesDirty = true;
String buddyNickname = "";
const int CHECKLIST_COUNT = 4;
String checklistItems[CHECKLIST_COUNT] = {
  "Review today's priority",
  "Check calendar",
  "Plan next focus block",
  "Wrap up notes"
};
bool checklistDone[CHECKLIST_COUNT] = {false, false, false, false};

enum HomeWidgetType {
  HOME_WIDGET_WEEK = 0,
  HOME_WIDGET_TIMER,
  HOME_WIDGET_RAIN,
  HOME_WIDGET_OUTDOOR,
  HOME_WIDGET_KP,
  HOME_WIDGET_UV,
  HOME_WIDGET_WIND,
  HOME_WIDGET_SUN
};

const int HOME_SLOT_COUNT = 4;
HomeWidgetType homeWidgetSlots[HOME_SLOT_COUNT] = {
  HOME_WIDGET_WEEK,
  HOME_WIDGET_TIMER,
  HOME_WIDGET_RAIN,
  HOME_WIDGET_OUTDOOR
};

String cacheHomeSlots[HOME_SLOT_COUNT];

// =========================================================
// STATE
// =========================================================
enum Page {
  PAGE_HOME = 0,
  PAGE_WEATHER = 1,
  PAGE_NOTES = 2,
  PAGE_STATUS = 3,
  PAGE_SETTINGS = 4
};

const int NAV_COUNT = 5;

Page currentPage = PAGE_HOME;
Page lastDrawnPage = (Page)-1;

unsigned long lastClockTick = 0;
unsigned long lastDataTick  = 0;

const unsigned long CLOCK_TICK_MS = 1000;
const unsigned long DATA_TICK_MS  = 30UL * 1000UL;

bool pageDirty = true;
bool dataDirty = true;

// cache
String cacheClock = "";
String cacheTemp = "";
String cacheRain = "";
String cacheWeek = "";
String cacheHomeEmpty1 = "";
String cacheHomeEmpty2 = "";
String cacheFocusTimer = "";
String cacheTimerMenu = "";
String cacheTimerDone = "";
String cacheTimerDoneCountdown = "";
String cacheTimerDoneFlash = "";
String cacheHomeHero = "";
String cacheHomeFocus = "";
String cacheHomeForecast = "";
String cacheHomeNote = "";
String cacheHomeTicker = "";
String cacheHomeShow = "";
int homeTickerOffset = 0;
unsigned long lastHomeTickerTick = 0;
const unsigned long HOME_TICKER_TICK_MS = 160UL;
int homeShowSlide = 0;
unsigned long lastHomeShowSlideMs = 0;
const int HOME_SHOW_SLIDE_COUNT = 5;
const unsigned long HOME_SHOW_SLIDE_MS = 12000UL;

String lastWifiText = "";
String lastSignalText = "";
String lastIpText = "";
String lastUptimeText = "";
String lastTempText = "";
String lastRainText = "";
String lastUvText = "";
String lastUvLevelText = "";
String lastKpText = "";
String lastKpLevelText = "";
String lastWindText = "";
String lastWindDirText = "";
String lastNextSunLabel = "";
String lastNextSunTime = "";
String lastNotesText = "";
String lastChecklistText = "";
String lastNetworkToggleText = "";
String lastSettingsText = "";
String lastInsightText = "";
String lastWeatherPanelText = "";

const char* homeWidgetKey(HomeWidgetType type) {
  switch (type) {
    case HOME_WIDGET_WEEK:    return "week";
    case HOME_WIDGET_TIMER:   return "timer";
    case HOME_WIDGET_RAIN:    return "rain";
    case HOME_WIDGET_OUTDOOR: return "outdoor";
    case HOME_WIDGET_KP:      return "kp";
    case HOME_WIDGET_UV:      return "uv";
    case HOME_WIDGET_WIND:    return "wind";
    case HOME_WIDGET_SUN:     return "sun";
    default:                  return "week";
  }
}

const char* homeWidgetLabel(HomeWidgetType type) {
  switch (type) {
    case HOME_WIDGET_WEEK:    return "Week";
    case HOME_WIDGET_TIMER:   return "Timer";
    case HOME_WIDGET_RAIN:    return "Rain";
    case HOME_WIDGET_OUTDOOR: return "Outdoor";
    case HOME_WIDGET_KP:      return "KP index";
    case HOME_WIDGET_UV:      return "UV index";
    case HOME_WIDGET_WIND:    return "Wind";
    case HOME_WIDGET_SUN:     return "Sun event";
    default:                  return "Week";
  }
}

HomeWidgetType homeWidgetFromKey(const String& key) {
  if (key == "week") return HOME_WIDGET_WEEK;
  if (key == "timer") return HOME_WIDGET_TIMER;
  if (key == "rain") return HOME_WIDGET_RAIN;
  if (key == "outdoor") return HOME_WIDGET_OUTDOOR;
  if (key == "kp") return HOME_WIDGET_KP;
  if (key == "uv") return HOME_WIDGET_UV;
  if (key == "wind") return HOME_WIDGET_WIND;
  if (key == "sun") return HOME_WIDGET_SUN;
  return HOME_WIDGET_WEEK;
}

const char* homeSlotLabel(int slot) {
  switch (slot) {
    case 0: return "Top left";
    case 1: return "Top right";
    case 2: return "Bottom left";
    case 3: return "Bottom right";
    default: return "Slot";
  }
}

void getHomeSlotRect(int slot, int& x, int& y, int& w, int& h) {
  const int xs[HOME_SLOT_COUNT] = {8, 124, 8, 124};
  const int ys[HOME_SLOT_COUNT] = {HOME_GRID_Y1, HOME_GRID_Y1, HOME_GRID_Y2, HOME_GRID_Y2};
  x = xs[slot];
  y = ys[slot];
  w = 108;
  h = HOME_WIDGET_H;
}

void appendHomeWidgetOptions(String& page, const String& selectedKey) {
  const HomeWidgetType types[] = {
    HOME_WIDGET_WEEK,
    HOME_WIDGET_TIMER,
    HOME_WIDGET_RAIN,
    HOME_WIDGET_OUTDOOR,
    HOME_WIDGET_KP,
    HOME_WIDGET_UV,
    HOME_WIDGET_WIND,
    HOME_WIDGET_SUN
  };

  for (HomeWidgetType type : types) {
    const char* key = homeWidgetKey(type);
    page += "<option value='";
    page += key;
    page += "'";
    if (selectedKey == key) page += " selected";
    page += ">";
    page += homeWidgetLabel(type);
    page += "</option>";
  }
}

void clearHomeSlotCaches() {
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    cacheHomeSlots[i] = "";
  }
}

// Focus timer
bool focusMenuOpen = false;
bool focusTimerRunning = false;
bool focusTimerFinished = false;
unsigned long focusEndMs = 0;
unsigned long focusDurationSec = 0;
unsigned long focusRemainingSec = 0;
bool timerDoneDialogOpen = false;
unsigned long timerDoneDialogStartedMs = 0;
const unsigned long TIMER_DONE_DIALOG_MS = 60UL * 1000UL;
bool flashModeEnabled = false;
int timerPresetMin[6] = {1, 5, 10, 15, 25, 30};
bool wifiEnabled = true;
bool wifiConnectInProgress = false;
unsigned long wifiConnectStartedMs = 0;
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000UL;
bool wifiPortalRequested = false;

struct CityPreset {
  const char* name;
  float lat;
  float lng;
};

const CityPreset CITY_PRESETS[] = {
  {"Berlin", 52.5200f, 13.4050f},
  {"Sao Paulo", -23.5505f, -46.6333f},
  {"New York", 40.7128f, -74.0060f},
  {"London", 51.5072f, -0.1276f},
  {"Tokyo", 35.6762f, 139.6503f},
  {"Lisbon", 38.7223f, -9.1393f}
};
const int CITY_PRESET_COUNT = sizeof(CITY_PRESETS) / sizeof(CITY_PRESETS[0]);

// Weather
const int HOURLY_FORECAST_COUNT = 6;
const int DAILY_FORECAST_COUNT = 7;

struct HourlyForecast {
  int minuteOfDay;
  float tempC;
  float precipProb;
  int weatherCode;
  bool valid;
};

struct DailyForecast {
  String label;
  float minC;
  float maxC;
  float precipProb;
  int weatherCode;
  bool valid;
};

static float tempC = NAN;
static float tempMinC = NAN;
static float tempMaxC = NAN;
static float precipMm = NAN;
static float windSpeedMs = NAN;
static float windDirectionDeg = NAN;
static float uvIndex = NAN;
static int weatherCode = -1;
HourlyForecast hourlyForecasts[HOURLY_FORECAST_COUNT];
DailyForecast dailyForecasts[DAILY_FORECAST_COUNT];
static time_t lastWeatherFetch = 0;
static const uint32_t WEATHER_INTERVAL_SEC = 10 * 60;

// KP-index
static float kpIndex = NAN;
static time_t lastKpFetch = 0;
static const uint32_t KP_INTERVAL_SEC = 10 * 60;

// Sunrise / Sunset
static int sunriseMin = -1;
static int sunsetMin  = -1;
static int lastSunYmd = -1;
static time_t lastSyncTime = 0;

// Public content
String contentMode = "mix"; // mix, quote, tech, off
String insightTitle = "Deskbuddy";
String insightBody = "Tap Setup to choose quotes, tech headlines, city, Wi-Fi, and brightness.";
String insightSource = "Local";
String insightStatus = "Waiting for Wi-Fi";
static time_t lastInsightFetch = 0;
static time_t lastInsightAttempt = 0;
static int insightFailureCount = 0;
static const uint32_t INSIGHT_INTERVAL_SEC = 60 * 60;
static const uint32_t INSIGHT_RETRY_SEC = 5 * 60;

// =========================================================
// SLEEP / BACKLIGHT
// =========================================================
const int BACKLIGHT_PIN = 21;

bool sleepDimmed = false;
bool sleepOff = false;
bool manualDimMode = false;

unsigned long lastInteractionMs = 0;

int sleepIntervalMin = 10;
int sleepOffDelaySec = 60;

int BL_FULL = 255;
const int BL_DIM  = 18;
const int BL_OFF  = 0;
const int FLASH_BL_LOW = 20;
const int FLASH_BL_HIGH = 255;

void wakeDisplay(bool clearManualMode = true);

int sanitizeTimerMinutes(int value);

// =========================================================
// HELPERS
// =========================================================
static int ymdFromLocal(time_t t) {
  struct tm tmLocal;
  localtime_r(&t, &tmLocal);
  return (tmLocal.tm_year + 1900) * 10000 + (tmLocal.tm_mon + 1) * 100 + tmLocal.tm_mday;
}

static int minutesFromLocalEpoch(time_t t) {
  struct tm tmLocal;
  localtime_r(&t, &tmLocal);
  return tmLocal.tm_hour * 60 + tmLocal.tm_min;
}

static int minutesNowLocal() {
  time_t now = time(nullptr);
  struct tm tmNow;
  localtime_r(&now, &tmNow);
  return tmNow.tm_hour * 60 + tmNow.tm_min;
}

static String wifiStatusText() {
  if (!wifiEnabled) return "Disabled";
  return WiFi.status() == WL_CONNECTED ? "Online" : "Offline";
}

static String signalText() {
  if (!wifiEnabled || WiFi.status() != WL_CONNECTED) return "-- dBm";
  return String(WiFi.RSSI()) + " dBm";
}

static String ipText() {
  if (!wifiEnabled || WiFi.status() != WL_CONNECTED) return "-";
  return WiFi.localIP().toString();
}

static bool useUsRegionFormat() {
  return regionFormatKey == "us";
}

static String formatClockParts(const struct tm& tmValue, bool withSeconds) {
  char buf[20];
  const char* pattern = useUsRegionFormat()
    ? (withSeconds ? "%I:%M:%S %p" : "%I:%M %p")
    : (withSeconds ? "%H:%M:%S" : "%H:%M");
  strftime(buf, sizeof(buf), pattern, &tmValue);
  return String(buf);
}

static String formatDateParts(const struct tm& tmValue) {
  char buf[32];
  strftime(buf, sizeof(buf), useUsRegionFormat() ? "%a %m/%d/%Y" : "%a %d.%m.%Y", &tmValue);
  return String(buf);
}

static String formatMinuteOfDay(int minOfDay) {
  if (minOfDay < 0) return "--:--";
  if (useUsRegionFormat()) {
    int hour24 = minOfDay / 60;
    int minute = minOfDay % 60;
    int hour12 = hour24 % 12;
    if (hour12 == 0) hour12 = 12;
    char buf[12];
    snprintf(buf, sizeof(buf), "%d:%02d %s", hour12, minute, hour24 >= 12 ? "PM" : "AM");
    return String(buf);
  }
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", minOfDay / 60, minOfDay % 60);
  return String(buf);
}

static String tempText() {
  if (isnan(tempC)) return unitKey == "imperial" ? "--.-F" : "--.-C";

  if (unitKey == "imperial") {
    float f = tempC * 9.0f / 5.0f + 32.0f;
    return String(f, 1) + "F";
  }

  return String(tempC, 1) + "C";
}

static String formatDisplayTemp(float value) {
  if (isnan(value)) return "--";

  if (unitKey == "imperial") {
    float f = value * 9.0f / 5.0f + 32.0f;
    return String((int)roundf(f)) + "F";
  }

  return String((int)roundf(value)) + "C";
}

static String formatCompactTemp(float value) {
  if (isnan(value)) return "--";

  if (unitKey == "imperial") {
    return String((int)roundf(value * 9.0f / 5.0f + 32.0f));
  }

  return String((int)roundf(value));
}

static String tempRangeText() {
  return "H:" + formatDisplayTemp(tempMaxC) + "  L:" + formatDisplayTemp(tempMinC);
}

static String rainText() {
  if (isnan(precipMm)) return unitKey == "imperial" ? "--.--in" : "--.-mm";

  if (unitKey == "imperial") {
    float inches = precipMm / 25.4f;
    return String(inches, 2) + "in";
  }

  return String(precipMm, 1) + "mm";
}

static String windText() {
  if (isnan(windSpeedMs)) return unitKey == "imperial" ? "--.-mph" : "--.-m/s";

  if (unitKey == "imperial") {
    float mph = windSpeedMs * 2.236936f;
    return String(mph, 1) + "mph";
  }

  return String(windSpeedMs, 1) + "m/s";
}

static String windDirectionText() {
  if (isnan(windDirectionDeg)) return "--";

  const char* dirs[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  int idx = (int)roundf(windDirectionDeg / 45.0f) % 8;
  return String(dirs[idx]) + " " + String((int)roundf(windDirectionDeg)) + "deg";
}

static String formatHourLabel(int minuteOfDay) {
  if (minuteOfDay < 0) return "--";
  int hour = minuteOfDay / 60;
  if (useUsRegionFormat()) {
    int hour12 = hour % 12;
    if (hour12 == 0) hour12 = 12;
    return String(hour12) + (hour >= 12 ? "p" : "a");
  }

  char buf[6];
  snprintf(buf, sizeof(buf), "%02d", hour);
  return String(buf);
}

static String weekdayLabelFromDate(const char* isoDate, int index) {
  if (index == 0) return "Today";
  if (!isoDate || strlen(isoDate) < 10) return "+" + String(index) + "d";

  struct tm tmDate;
  memset(&tmDate, 0, sizeof(tmDate));
  tmDate.tm_year = String(isoDate).substring(0, 4).toInt() - 1900;
  tmDate.tm_mon = String(isoDate).substring(5, 7).toInt() - 1;
  tmDate.tm_mday = String(isoDate).substring(8, 10).toInt();
  tmDate.tm_isdst = -1;
  mktime(&tmDate);

  char buf[8];
  strftime(buf, sizeof(buf), "%a", &tmDate);
  return String(buf);
}

static String weatherConditionText(int code) {
  if (code < 0) return "Updating";
  if (code == 0) return "Clear";
  if (code <= 3) return "Cloudy";
  if (code == 45 || code == 48) return "Fog";
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return "Rain";
  if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86)) return "Snow";
  if (code >= 95) return "Storm";
  return "Weather";
}

static String weatherShortText(int code) {
  String label = weatherConditionText(code);
  if (label == "Updating") return "--";
  return label;
}

struct DailyReading {
  const char* ref;
  const char* text;
  const char* prompt;
};

static const DailyReading DAILY_READINGS[] = {
  {"Psalm 23", "The Lord guides, restores strength, and keeps the path steady.", "Start with one calm step."},
  {"Psalm 46", "God is a present refuge when the day feels noisy or heavy.", "Pause, breathe, continue."},
  {"Psalm 91", "Rest under God's care; courage grows when fear gets smaller.", "Choose trust before speed."},
  {"Psalm 121", "Help comes from the Maker who watches over every ordinary hour.", "Look up before reacting."},
  {"Proverbs 3", "Wisdom begins with trust, humility, and a straight path.", "Make the next decision clean."},
  {"Matthew 6", "Today has enough weight; receive grace for this day.", "Do today well."},
  {"Philippians 4", "Peace guards the heart when prayer replaces anxious loops.", "Turn worry into prayer."},
  {"Isaiah 40", "Those who wait on the Lord renew strength for the road ahead.", "Renew, then move."},
  {"Psalm 27", "Light and courage are stronger than fear.", "Stand firm with a quiet heart."},
  {"James 1", "Ask for wisdom with faith, and walk with patience.", "Choose wisdom over hurry."},
  {"Romans 12", "Renew the mind and serve with practical love.", "Do useful good today."},
  {"Psalm 34", "The Lord is near to the brokenhearted and attentive to humble prayer.", "Bring the hard part to God."},
  {"Colossians 3", "Work with sincerity, gratitude, and a higher purpose.", "Make the work worshipful."},
  {"John 14", "Do not let the heart be troubled; peace is offered again.", "Receive peace before tasks."}
};

static int dailyReadingIndex() {
  time_t now = time(nullptr);
  if (now < 1600000000) return 0;
  struct tm tmNow;
  localtime_r(&now, &tmNow);
  return tmNow.tm_yday % (sizeof(DAILY_READINGS) / sizeof(DAILY_READINGS[0]));
}

static DailyReading dailyReading() {
  return DAILY_READINGS[dailyReadingIndex()];
}

static String kpText() {
  return isnan(kpIndex) ? "Kp --" : "Kp " + String(kpIndex, 1);
}

static String kpLevelText() {
  if (isnan(kpIndex)) return "--";
  if (kpIndex < 3.0f) return "Low";
  if (kpIndex < 5.0f) return "Medium";
  if (kpIndex < 7.0f) return "High";
  return "Extreme";
}

static String uvText() {
  return isnan(uvIndex) ? "UV --" : "UV " + String(uvIndex, 1);
}

static String uvLevelText() {
  if (isnan(uvIndex)) return "--";
  if (uvIndex < 3.0f) return "Low";
  if (uvIndex < 6.0f) return "Moderate";
  if (uvIndex < 8.0f) return "High";
  if (uvIndex < 11.0f) return "Very High";
  return "Extreme";
}

static uint16_t statusColor() {
  if (textColorKey != "standard") return COL_TEXT;
  if (!wifiEnabled) return COL_YELLOW;
  return WiFi.status() == WL_CONNECTED ? COL_GREEN : COL_RED;
}

static String uptimeText() {
  unsigned long seconds = millis() / 1000UL;
  unsigned long days = seconds / 86400UL;
  seconds %= 86400UL;
  unsigned long hours = seconds / 3600UL;
  seconds %= 3600UL;
  unsigned long minutes = seconds / 60UL;

  if (days > 0) return String(days) + "d " + String(hours) + "h";
  if (hours > 0) return String(hours) + "h " + String(minutes) + "m";
  return String(minutes) + "m";
}

static String nextSunLabel() {
  int nowMin = minutesNowLocal();
  if (sunriseMin < 0 || sunsetMin < 0) return "Sun";
  if (nowMin < sunriseMin) return "Sunrise";
  if (nowMin < sunsetMin) return "Sunset";
  return "Sunrise";
}

static String nextSunTimeText() {
  int nowMin = minutesNowLocal();
  if (sunriseMin < 0 || sunsetMin < 0) return "--:--";
  if (nowMin < sunriseMin) return formatMinuteOfDay(sunriseMin);
  if (nowMin < sunsetMin) return formatMinuteOfDay(sunsetMin);
  return formatMinuteOfDay(sunriseMin);
}

static String htmlEscape(const String& s) {
  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') out += "&amp;";
    else if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;";
    else if (c == '"') out += "&quot;";
    else out += c;
  }
  return out;
}

static String cssColorFrom565(uint16_t color) {
  uint8_t r = ((color >> 11) & 0x1F) * 255 / 31;
  uint8_t g = ((color >> 5) & 0x3F) * 255 / 63;
  uint8_t b = (color & 0x1F) * 255 / 31;
  char buf[8];
  snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
  return String(buf);
}

static String accentPreviewCss(const String& key) {
  if (key == "standard") return cssColorFrom565(0xEF7D);
  if (key == "ice")      return cssColorFrom565(0xEFFF);
  if (key == "white")    return cssColorFrom565(TFT_WHITE);
  if (key == "cyan")     return cssColorFrom565(0x5EFA);
  if (key == "mint")     return cssColorFrom565(0x07F0);
  if (key == "green")    return cssColorFrom565(TFT_GREEN);
  if (key == "blue")     return cssColorFrom565(0x3D9F);
  if (key == "purple")   return cssColorFrom565(0xA2F5);
  if (key == "pink")     return cssColorFrom565(0xF97F);
  if (key == "orange")   return cssColorFrom565(0xFD20);
  if (key == "amber")    return cssColorFrom565(0xFEA0);
  if (key == "red")      return cssColorFrom565(TFT_RED);
  return cssColorFrom565(0xEF7D);
}

static String themePreviewCss(const String& key) {
  if (key == "platinum") return cssColorFrom565(0xD6BA);
  if (key == "cupertino") return cssColorFrom565(0x0259);
  if (key == "glass")    return cssColorFrom565(0x2945);
  if (key == "slate")    return cssColorFrom565(0x08A3);
  if (key == "deep")     return cssColorFrom565(0x0000);
  if (key == "nordic")   return cssColorFrom565(0x0864);
  if (key == "forest")   return cssColorFrom565(0x0208);
  if (key == "coffee")   return cssColorFrom565(0x18A3);
  if (key == "soft")     return cssColorFrom565(0x10A2);
  if (key == "midnight") return cssColorFrom565(0x0008);
  if (key == "graphite") return cssColorFrom565(0x1082);
  if (key == "garnet")   return cssColorFrom565(0x1004);
  if (key == "ochre")    return cssColorFrom565(0x20E1);
  return cssColorFrom565(0x08A3);
}

static String formatTimerClock(unsigned long totalSec) {
  unsigned long minutes = totalSec / 60UL;
  unsigned long seconds = totalSec % 60UL;

  char buf[10];
  snprintf(buf, sizeof(buf), "%02lu:%02lu", minutes, seconds);
  return String(buf);
}

static String focusHintText() {
  if (focusTimerFinished) return "Tap to reset";
  if (focusTimerRunning) return String((focusDurationSec / 60UL)) + " min session";
  return "Tap to start";
}

static String formatElapsedText(unsigned long totalSec) {
  unsigned long minutes = totalSec / 60UL;
  if (minutes == 0) return "< 1 minute elapsed";
  if (minutes == 1) return "1 minute elapsed";
  return String(minutes) + " minutes elapsed";
}

static String lastSyncText() {
  if (lastSyncTime <= 0) return "Sync --:--";

  struct tm tmSync;
  localtime_r(&lastSyncTime, &tmSync);
  return "Sync " + formatClockParts(tmSync, false);
}

static String weekNumberText() {
  time_t now = time(nullptr);
  struct tm tmNow;
  localtime_r(&now, &tmNow);
  char buf[4];
  strftime(buf, sizeof(buf), "%V", &tmNow);
  return String(buf);
}

static String timerDoneCountdownText() {
  if (!timerDoneDialogOpen) return "";

  unsigned long elapsedMs = millis() - timerDoneDialogStartedMs;
  unsigned long remainingMs = (elapsedMs >= TIMER_DONE_DIALOG_MS) ? 0 : (TIMER_DONE_DIALOG_MS - elapsedMs);
  unsigned long remainingSec = (remainingMs + 999UL) / 1000UL;
  return String("Auto close in ") + String(remainingSec) + "s";
}

static String homeTitleText() {
  return buddyNickname.length() > 0 ? buddyNickname : "Deskbuddy";
}

static String homeTickerText() {
  String source = insightSource.length() > 0 ? insightSource : "Live";
  String body = insightBody.length() > 0 ? insightBody : "Use Setup to enable quotes or tech headlines.";
  body.replace("\n", " ");
  body.trim();
  return source + "  -  " + body;
}

static String compactNoteText() {
  String text = notesText;
  text.replace("\n", " ");
  text.trim();
  if (text.length() == 0) text = "No notes yet.";
  if (text.length() > 72) text = text.substring(0, 69) + "...";
  return text;
}

int sanitizeTimerMinutes(int value) {
  return constrain(value, 1, 180);
}

void resetFocusTimer() {
  focusTimerRunning = false;
  focusTimerFinished = false;
  focusMenuOpen = false;
  timerDoneDialogOpen = false;
  focusEndMs = 0;
  focusDurationSec = 0;
  focusRemainingSec = 0;
  cacheFocusTimer = "";
  clearHomeSlotCaches();
  cacheTimerMenu = "";
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  cacheTimerDoneFlash = "";
}

void startFocusTimer(unsigned long minutes) {
  focusDurationSec = minutes * 60UL;
  focusRemainingSec = focusDurationSec;
  focusEndMs = millis() + (focusDurationSec * 1000UL);
  focusTimerRunning = true;
  focusTimerFinished = false;
  focusMenuOpen = false;
  timerDoneDialogOpen = false;
  cacheFocusTimer = "";
  clearHomeSlotCaches();
  cacheTimerMenu = "";
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  cacheTimerDoneFlash = "";
}

void dismissTimerDoneDialog() {
  if (!timerDoneDialogOpen) return;
  timerDoneDialogOpen = false;
  timerDoneDialogStartedMs = 0;
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  cacheTimerDoneFlash = "";
  if (!sleepDimmed && !sleepOff) setBacklight(BL_FULL);
  pageDirty = true;
}

void openTimerDoneDialog() {
  timerDoneDialogOpen = true;
  timerDoneDialogStartedMs = millis();
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  wakeDisplay();
}

void updateFocusTimerState() {
  if (!focusTimerRunning) return;

  unsigned long now = millis();
  if ((long)(focusEndMs - now) <= 0) {
    focusRemainingSec = 0;
    focusTimerRunning = false;
    focusTimerFinished = true;
    focusMenuOpen = false;
    cacheFocusTimer = "";
    clearHomeSlotCaches();
    cacheTimerMenu = "";
    openTimerDoneDialog();
    return;
  }

  unsigned long remainingMs = focusEndMs - now;
  unsigned long nextRemainingSec = (remainingMs + 999UL) / 1000UL;
  if (nextRemainingSec != focusRemainingSec) {
    focusRemainingSec = nextRemainingSec;
    cacheFocusTimer = "";
    clearHomeSlotCaches();
  }
}

void updateTimerDoneDialogState() {
  if (!timerDoneDialogOpen) return;
  if (millis() - timerDoneDialogStartedMs >= TIMER_DONE_DIALOG_MS) {
    dismissTimerDoneDialog();
  }
}

void setBacklight(int value) {
  value = constrain(value, 0, 255);
  analogWrite(BACKLIGHT_PIN, value);
}

void wakeDisplay(bool clearManualMode) {
  sleepDimmed = false;
  sleepOff = false;
  if (clearManualMode) manualDimMode = false;
  lastInteractionMs = millis();
  setBacklight(BL_FULL);
  pageDirty = true;
}

void enterSleepDim() {
  if (sleepDimmed || sleepOff || manualDimMode) return;
  sleepDimmed = true;
  setBacklight(BL_DIM);
}

void enterSleepOff() {
  if (sleepOff) return;
  sleepOff = true;
  sleepDimmed = true;
  setBacklight(BL_OFF);
  pageDirty = true;
}

void toggleSleepMode() {
  if (manualDimMode) {
    wakeDisplay();
    return;
  }

  manualDimMode = true;
  sleepDimmed = true;
  sleepOff = false;
  setBacklight(BL_DIM);
  pageDirty = true;
}

void handleAutoSleep() {
  if (focusMenuOpen || timerDoneDialogOpen) return;
  if (sleepIntervalMin <= 0) return;

  unsigned long now = millis();
  unsigned long dimAfterMs = (unsigned long)sleepIntervalMin * 60UL * 1000UL;
  unsigned long offAfterMs = dimAfterMs + ((unsigned long)sleepOffDelaySec * 1000UL);

  if (!sleepDimmed && !sleepOff && now - lastInteractionMs > dimAfterMs) {
    enterSleepDim();
  }

  if (sleepDimmed && !sleepOff && now - lastInteractionMs > offAfterMs) {
    enterSleepOff();
  }
}

// =========================================================
// THEME / SETTINGS
// =========================================================
void applyThemeByKey(const String& accentKey, const String& bgKey) {
  if (accentKey == "standard")    COL_ACCENT = 0xEF7D;
  else if (accentKey == "cyan")   COL_ACCENT = 0x5EFA;
  else if (accentKey == "ice")    COL_ACCENT = 0xEFFF;
  else if (accentKey == "white")  COL_ACCENT = TFT_WHITE;
  else if (accentKey == "mint")   COL_ACCENT = 0x07F0;
  else if (accentKey == "green")  COL_ACCENT = TFT_GREEN;
  else if (accentKey == "blue")   COL_ACCENT = 0x3D9F;
  else if (accentKey == "purple") COL_ACCENT = 0xA2F5;
  else if (accentKey == "pink")   COL_ACCENT = 0xF97F;
  else if (accentKey == "orange") COL_ACCENT = 0xFD20;
  else if (accentKey == "amber")  COL_ACCENT = 0xFEA0;
  else if (accentKey == "red")    COL_ACCENT = TFT_RED;
  else                            COL_ACCENT = 0x5EFA;

  if (bgKey == "slate") {
    COL_BG = 0x08A3; COL_PANEL = 0x1106; COL_PANEL_ALT = 0x18C7; COL_STROKE = 0x31EC;
  } else if (bgKey == "deep") {
    COL_BG = 0x0000; COL_PANEL = 0x0841; COL_PANEL_ALT = 0x1082; COL_STROKE = 0x2945;
  } else if (bgKey == "nordic") {
    COL_BG = 0x0864; COL_PANEL = 0x10C6; COL_PANEL_ALT = 0x1908; COL_STROKE = 0x3A2D;
  } else if (bgKey == "forest") {
    COL_BG = 0x0208; COL_PANEL = 0x0ACB; COL_PANEL_ALT = 0x134D; COL_STROKE = 0x2D72;
  } else if (bgKey == "coffee") {
    COL_BG = 0x18A3; COL_PANEL = 0x2945; COL_PANEL_ALT = 0x39C7; COL_STROKE = 0x5A89;
  } else if (bgKey == "soft") {
    COL_BG = 0x10A2; COL_PANEL = 0x1924; COL_PANEL_ALT = 0x2145; COL_STROKE = 0x3A49;
  } else if (bgKey == "midnight") {
    COL_BG = 0x0008; COL_PANEL = 0x0011; COL_PANEL_ALT = 0x0018; COL_STROKE = 0x3A7F;
  } else if (bgKey == "graphite") {
    COL_BG = 0x1082; COL_PANEL = 0x18C3; COL_PANEL_ALT = 0x2104; COL_STROKE = 0x4208;
  } else if (bgKey == "garnet") {
    COL_BG = 0x1004; COL_PANEL = 0x1886; COL_PANEL_ALT = 0x20E8; COL_STROKE = 0x41AC;
  } else if (bgKey == "ochre") {
    COL_BG = 0x20E1; COL_PANEL = 0x3184; COL_PANEL_ALT = 0x4226; COL_STROKE = 0x632B;
  } else if (bgKey == "platinum") {
    COL_BG = 0xD6BA; COL_PANEL = 0xEF7D; COL_PANEL_ALT = 0xC638; COL_STROKE = 0xA514;
  } else if (bgKey == "cupertino") {
    COL_BG = 0x0259; COL_PANEL = 0x0B1D; COL_PANEL_ALT = 0x123E; COL_STROKE = 0x3CDF;
  } else if (bgKey == "glass") {
    COL_BG = 0x18E3; COL_PANEL = 0x2945; COL_PANEL_ALT = 0x3186; COL_STROKE = 0x5AEB;
  } else {
    COL_BG = 0x1082; COL_PANEL = 0x18C3; COL_PANEL_ALT = 0x2104; COL_STROKE = 0x4208;
  }
}

void applyTextColorByKey(const String& key) {
  textColorKey = key;

  if (key == "standard") {
    COL_TEXT = 0xEF7D; COL_DIM  = 0x94B2;
  } else if (key == "white") {
    COL_TEXT = TFT_WHITE; COL_DIM = 0xBDF7;
  } else if (key == "ice") {
    COL_TEXT = 0xEFFF; COL_DIM = 0x9D7F;
  } else if (key == "mint") {
    COL_TEXT = 0x07F0; COL_DIM = 0x05EC;
  } else if (key == "orange") {
    COL_TEXT = 0xFD20; COL_DIM = 0xBA26;
  } else if (key == "amber") {
    COL_TEXT = 0xFEA0; COL_DIM = 0xBCE0;
  } else if (key == "green") {
    COL_TEXT = TFT_GREEN; COL_DIM = 0x86E8;
  } else if (key == "cyan") {
    COL_TEXT = 0x5EFA; COL_DIM = 0x3D96;
  } else if (key == "blue") {
    COL_TEXT = 0x3D9F; COL_DIM = 0x22B1;
  } else if (key == "purple") {
    COL_TEXT = 0xA2F5; COL_DIM = 0x79ED;
  } else if (key == "red") {
    COL_TEXT = TFT_RED; COL_DIM = 0xB9E7;
  } else if (key == "pink") {
    COL_TEXT = 0xF97F; COL_DIM = 0xC2F1;
  } else {
    COL_TEXT = 0xEF7D;
    COL_DIM  = 0x94B2;
    textColorKey = "standard";
  }
}

void loadStoredSettings() {
  prefs.begin("deskbuddy", false);

  String accent = prefs.getString("accent", "ice");
  String bg     = prefs.getString("bg", "graphite");
  String txt    = prefs.getString("text", "standard");

  notesText        = prefs.getString("notes", "No notes yet.");
  buddyNickname    = prefs.getString("nickname", "");
  locationName     = prefs.getString("locname", "Berlin");
  LAT              = prefs.getFloat("lat", 52.5200f);
  LNG              = prefs.getFloat("lng", 13.4050f);
  sleepIntervalMin = prefs.getInt("sleepMin", 10);
  unitKey          = prefs.getString("units", "metric");
  regionFormatKey  = prefs.getString("region", "europe");
  flashModeEnabled = prefs.getBool("flashMode", false);
  wifiEnabled      = prefs.getBool("wifiEnabled", true);
  BL_FULL          = constrain(prefs.getInt("brightness", 255), 30, 255);
  contentMode      = prefs.getString("contentMode", "mix");
  if (contentMode != "mix" && contentMode != "quote" && contentMode != "tech" && contentMode != "off") contentMode = "mix";
  int liveSchema = prefs.getInt("liveSchema", 0);
  if (liveSchema < 1 && contentMode != "off") {
    contentMode = "mix";
    prefs.putString("contentMode", contentMode);
    prefs.putInt("liveSchema", 1);
  }

  for (int i = 0; i < CHECKLIST_COUNT; i++) {
    String textKey = String("checkText") + String(i);
    String doneKey = String("checkDone") + String(i);
    checklistItems[i] = prefs.getString(textKey.c_str(), checklistItems[i]);
    checklistItems[i].trim();
    if (checklistItems[i].length() == 0) checklistItems[i] = "Task " + String(i + 1);
    if (checklistItems[i].length() > 40) checklistItems[i] = checklistItems[i].substring(0, 40);
    checklistDone[i] = prefs.getBool(doneKey.c_str(), checklistDone[i]);
  }

  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    String key = String("homeSlot") + String(i);
    homeWidgetSlots[i] = homeWidgetFromKey(prefs.getString(key.c_str(), homeWidgetKey(homeWidgetSlots[i])));
  }

  for (int i = 0; i < 6; i++) {
    String key = String("timer") + String(i);
    timerPresetMin[i] = sanitizeTimerMinutes(prefs.getInt(key.c_str(), timerPresetMin[i]));
  }

  if (unitKey != "metric" && unitKey != "imperial") unitKey = "metric";
  if (regionFormatKey != "europe" && regionFormatKey != "us") regionFormatKey = "europe";
  buddyNickname.trim();
  applyThemeByKey(accent, bg);
  applyTextColorByKey(txt);
}

void resetDataCaches() {
  tempC = NAN;
  tempMinC = NAN;
  tempMaxC = NAN;
  precipMm = NAN;
  windSpeedMs = NAN;
  windDirectionDeg = NAN;
  uvIndex = NAN;
  weatherCode = -1;
  for (int i = 0; i < HOURLY_FORECAST_COUNT; i++) hourlyForecasts[i].valid = false;
  for (int i = 0; i < DAILY_FORECAST_COUNT; i++) dailyForecasts[i].valid = false;
  kpIndex = NAN;
  sunriseMin = -1;
  sunsetMin = -1;
  lastSunYmd = -1;
  lastWeatherFetch = 0;
  lastKpFetch = 0;
  lastInsightFetch = 0;
  lastInsightAttempt = 0;
  dataDirty = true;
  pageDirty = true;
}

// =========================================================
// TOUCH
// =========================================================
bool readTouchXY(int& sx, int& sy) {
  if (!ts.touched()) return false;

  TS_Point p = ts.getPoint();
  if (p.z < 80 || p.z > 4000) return false;

  int x = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_W);
  int y = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_H);

  x = constrain(x, 0, SCREEN_W - 1);
  y = constrain(y, 0, SCREEN_H - 1);

  if (TOUCH_SWAP_XY) { int tmp = x; x = y; y = tmp; }
  if (TOUCH_FLIP_X)  x = (SCREEN_W - 1) - x;
  if (TOUCH_FLIP_Y)  y = (SCREEN_H - 1) - y;

  sx = x;
  sy = y;
  return true;
}

bool touchNewPress(int& tx, int& ty) {
  static bool wasDown = false;
  static unsigned long lastPressMs = 0;

  bool down = false;
  int x = 0, y = 0;

  if (readTouchXY(x, y)) down = true;

  bool fire = false;
  unsigned long now = millis();

  if (down && !wasDown && (now - lastPressMs > 220)) {
    fire = true;
    lastPressMs = now;
    tx = x;
    ty = y;
  }

  wasDown = down;
  return fire;
}

// =========================================================
// API
// =========================================================
bool fetchSunriseSunset() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();

  String url = String("https://api.sunrise-sunset.org/json?lat=") + String(LAT, 4) +
               "&lng=" + String(LNG, 4) + "&formatted=0";

  HTTPClient http;
  if (!http.begin(client, url)) return false;

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, body)) return false;

  const char* sunriseStr = doc["results"]["sunrise"];
  const char* sunsetStr  = doc["results"]["sunset"];
  if (!sunriseStr || !sunsetStr) return false;

  auto parseIsoToEpochUTC = [](const char* iso) -> time_t {
    int Y, M, D, h, m, s;
    if (sscanf(iso, "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &s) != 6) return (time_t)-1;

    struct tm t{};
    t.tm_year = Y - 1900;
    t.tm_mon  = M - 1;
    t.tm_mday = D;
    t.tm_hour = h;
    t.tm_min  = m;
    t.tm_sec  = s;

    char* oldTz = getenv("TZ");
    String old = oldTz ? String(oldTz) : String("");

    setenv("TZ", "UTC0", 1);
    tzset();
    time_t epoch = mktime(&t);

    if (old.length()) setenv("TZ", old.c_str(), 1);
    else unsetenv("TZ");
    tzset();

    return epoch;
  };

  time_t srEpoch = parseIsoToEpochUTC(sunriseStr);
  time_t ssEpoch = parseIsoToEpochUTC(sunsetStr);
  if (srEpoch < 0 || ssEpoch < 0) return false;

  sunriseMin = minutesFromLocalEpoch(srEpoch);
  sunsetMin  = minutesFromLocalEpoch(ssEpoch);
  lastSunYmd = ymdFromLocal(time(nullptr));
  lastSyncTime = time(nullptr);
  return true;
}

void ensureSunTimesForToday() {
  time_t nowT = time(nullptr);
  int ymd = ymdFromLocal(nowT);

  if ((sunriseMin < 0 || sunsetMin < 0 || ymd != lastSunYmd) &&
      WiFi.status() == WL_CONNECTED) {
    if (fetchSunriseSunset()) dataDirty = true;
  }
}

bool fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();

  String url = String("https://api.open-meteo.com/v1/forecast?latitude=") + String(LAT, 4) +
               "&longitude=" + String(LNG, 4) +
               "&current=temperature_2m,wind_speed_10m,wind_direction_10m,uv_index,weather_code" +
               "&hourly=temperature_2m,precipitation,precipitation_probability,weather_code" +
               "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max" +
               "&forecast_days=7&timezone=auto&wind_speed_unit=ms";

  HTTPClient http;
  if (!http.begin(client, url)) return false;

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  DynamicJsonDocument doc(24576);
  if (deserializeJson(doc, body)) return false;

  tempC = doc["current"]["temperature_2m"] | NAN;
  windSpeedMs = doc["current"]["wind_speed_10m"] | NAN;
  windDirectionDeg = doc["current"]["wind_direction_10m"] | NAN;
  uvIndex = doc["current"]["uv_index"] | NAN;
  weatherCode = doc["current"]["weather_code"] | -1;
  tempMaxC = NAN;
  tempMinC = NAN;

  JsonArray maxTemps = doc["daily"]["temperature_2m_max"];
  JsonArray minTemps = doc["daily"]["temperature_2m_min"];
  JsonArray dailyCodes = doc["daily"]["weather_code"];
  JsonArray dailyRain = doc["daily"]["precipitation_probability_max"];
  JsonArray dailyTimes = doc["daily"]["time"];
  if (maxTemps && !maxTemps.isNull() && maxTemps.size() > 0) tempMaxC = maxTemps[0] | NAN;
  if (minTemps && !minTemps.isNull() && minTemps.size() > 0) tempMinC = minTemps[0] | NAN;

  for (int i = 0; i < DAILY_FORECAST_COUNT; i++) {
    dailyForecasts[i].valid = false;
    if (!dailyTimes || !maxTemps || !minTemps || i >= (int)dailyTimes.size()) continue;
    const char* dateText = dailyTimes[i];
    dailyForecasts[i].label = weekdayLabelFromDate(dateText, i);
    dailyForecasts[i].maxC = maxTemps[i] | NAN;
    dailyForecasts[i].minC = minTemps[i] | NAN;
    dailyForecasts[i].precipProb = dailyRain ? (dailyRain[i] | NAN) : NAN;
    dailyForecasts[i].weatherCode = dailyCodes ? (dailyCodes[i] | -1) : -1;
    dailyForecasts[i].valid = true;
  }

  JsonArray times = doc["hourly"]["time"];
  JsonArray precs = doc["hourly"]["precipitation"];
  JsonArray hourlyTemps = doc["hourly"]["temperature_2m"];
  JsonArray hourlyRain = doc["hourly"]["precipitation_probability"];
  JsonArray hourlyCodes = doc["hourly"]["weather_code"];

  for (int i = 0; i < HOURLY_FORECAST_COUNT; i++) hourlyForecasts[i].valid = false;

  if (times && precs) {
    time_t nowT = time(nullptr);
    struct tm tmNow;
    localtime_r(&nowT, &tmNow);

    char key[20];
    strftime(key, sizeof(key), "%Y-%m-%dT%H:00", &tmNow);

    int idx = -1;
    for (int i = 0; i < (int)times.size(); i++) {
      const char* t = times[i];
      if (t && String(t).startsWith(key)) {
        idx = i;
        break;
      }
    }
    if (idx < 0) idx = 0;
    precipMm = precs[idx] | NAN;

    for (int i = 0; i < HOURLY_FORECAST_COUNT; i++) {
      int src = idx + i;
      if (src >= (int)times.size()) break;
      const char* hourText = times[src];
      if (!hourText || strlen(hourText) < 16) continue;
      int hour = String(hourText).substring(11, 13).toInt();
      int minute = String(hourText).substring(14, 16).toInt();
      hourlyForecasts[i].minuteOfDay = hour * 60 + minute;
      hourlyForecasts[i].tempC = hourlyTemps ? (hourlyTemps[src] | NAN) : NAN;
      hourlyForecasts[i].precipProb = hourlyRain ? (hourlyRain[src] | NAN) : NAN;
      hourlyForecasts[i].weatherCode = hourlyCodes ? (hourlyCodes[src] | -1) : -1;
      hourlyForecasts[i].valid = true;
    }
  }

  lastWeatherFetch = time(nullptr);
  lastSyncTime = lastWeatherFetch;
  return true;
}

void ensureWeather() {
  time_t nowT = time(nullptr);
  if ((isnan(tempC) || isnan(tempMinC) || isnan(tempMaxC) || isnan(precipMm) || isnan(windSpeedMs) || isnan(windDirectionDeg) || isnan(uvIndex) || weatherCode < 0 ||
       (nowT - lastWeatherFetch) > WEATHER_INTERVAL_SEC) &&
      WiFi.status() == WL_CONNECTED) {
    if (fetchWeather()) dataDirty = true;
  }
}

bool httpGetJson(const String& url, JsonDocument& doc) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, url)) return false;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(12000);
  http.addHeader("User-Agent", "Deskbuddy ESP32");

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();
  return deserializeJson(doc, body) == DeserializationError::Ok;
}

bool fetchQuoteInsight() {
  {
    DynamicJsonDocument doc(3072);
    if (httpGetJson("https://zenquotes.io/api/random", doc)) {
      JsonArray arr = doc.as<JsonArray>();
      if (arr && arr.size() > 0) {
        const char* quote = arr[0]["q"] | nullptr;
        const char* author = arr[0]["a"] | "Unknown";
        if (quote) {
          insightTitle = String(author);
          insightBody = String(quote);
          insightSource = "ZenQuotes";
          return true;
        }
      }
    }
  }

  {
    DynamicJsonDocument doc(3072);
    if (httpGetJson("https://api.quotable.io/random?maxLength=140", doc)) {
      const char* content = doc["content"] | nullptr;
      const char* author = doc["author"] | "Unknown";
      if (content) {
        insightTitle = String(author);
        insightBody = String(content);
        insightSource = "Quotable";
        return true;
      }
    }
  }

  {
    DynamicJsonDocument doc(3072);
    if (httpGetJson("https://api.adviceslip.com/advice", doc)) {
      const char* advice = doc["slip"]["advice"] | nullptr;
      if (advice) {
        insightTitle = "Workday prompt";
        insightBody = String(advice);
        insightSource = "AdviceSlip";
        return true;
      }
    }
  }

  return false;
}

bool fetchTechInsight() {
  DynamicJsonDocument doc(8192);
  if (!httpGetJson("https://hn.algolia.com/api/v1/search_by_date?tags=story&query=technology", doc)) return false;

  JsonArray hits = doc["hits"];
  if (!hits || hits.size() == 0) return false;
  const char* title = hits[0]["title"] | hits[0]["story_title"] | nullptr;
  const char* author = hits[0]["author"] | "HN";
  if (!title) return false;
  insightTitle = "Tech pulse";
  insightBody = String(title);
  insightSource = String("Hacker News / ") + String(author);
  return true;
}

void setLocalInsightFallback() {
  const char* fallbackQuotes[] = {
    "Focus on the next useful step.",
    "Make the important thing easier to start.",
    "Small improvements compound during a long workday.",
    "Clear context beats a crowded dashboard."
  };
  int idx = (millis() / 1000UL) % 4;
  insightTitle = "Deskbuddy";
  insightBody = fallbackQuotes[idx];
  insightSource = "Local fallback";
}

bool fetchInsight() {
  if (WiFi.status() != WL_CONNECTED || contentMode == "off") return false;
  lastInsightAttempt = time(nullptr);
  bool fetchTech = contentMode == "tech" || (contentMode == "mix" && ((lastInsightAttempt / INSIGHT_RETRY_SEC) % 2 == 0));
  insightStatus = fetchTech ? "Fetching tech news..." : "Fetching quote...";

  bool ok = fetchTech ? fetchTechInsight() : fetchQuoteInsight();
  if (!ok && contentMode == "mix") ok = fetchTech ? fetchQuoteInsight() : fetchTechInsight();
  if (!ok) {
    insightFailureCount++;
    if (insightFailureCount >= 1 || insightBody.length() == 0 || insightSource == "Local") setLocalInsightFallback();
    insightStatus = "Live fetch failed; retrying";
    notesDirty = true;
    pageDirty = true;
    return false;
  }

  if (insightBody.length() > 180) insightBody = insightBody.substring(0, 177) + "...";
  lastInsightFetch = time(nullptr);
  lastSyncTime = lastInsightFetch;
  insightFailureCount = 0;
  struct tm tmInsight;
  localtime_r(&lastInsightFetch, &tmInsight);
  insightStatus = "Live updated " + formatClockParts(tmInsight, false);
  return true;
}

void ensureInsight() {
  if (contentMode == "off") {
    insightTitle = "Live card off";
    insightBody = "Enable quotes or tech headlines from Setup.";
    insightSource = "Local";
    insightStatus = "Off";
    return;
  }

  time_t nowT = time(nullptr);
  if (WiFi.status() != WL_CONNECTED) {
    insightTitle = "Waiting for Wi-Fi";
    insightBody = "Connect Wi-Fi to load quotes and tech news.";
    insightSource = "Network";
    insightStatus = "Offline";
    return;
  }

  bool due = lastInsightFetch > 0 && (nowT - lastInsightFetch) > INSIGHT_INTERVAL_SEC;
  bool retryDue = lastInsightFetch == 0 && (lastInsightAttempt == 0 || (nowT - lastInsightAttempt) > INSIGHT_RETRY_SEC);
  if ((due || retryDue) &&
      WiFi.status() == WL_CONNECTED) {
    if (fetchInsight()) {
      notesDirty = true;
      dataDirty = true;
      pageDirty = true;
    }
  }
}

bool fetchKpIndex() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, "https://services.swpc.noaa.gov/products/noaa-planetary-k-index.json")) {
    return false;
  }

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  int lastRow = body.lastIndexOf('[');
  if (lastRow < 0) return false;

  int firstComma = body.indexOf(',', lastRow);
  if (firstComma < 0) return false;

  int q1 = body.indexOf('"', firstComma);
  if (q1 < 0) return false;
  int q2 = body.indexOf('"', q1 + 1);
  if (q2 < 0) return false;

  String kpStrLocal = body.substring(q1 + 1, q2);
  kpIndex = kpStrLocal.toFloat();

  lastKpFetch = time(nullptr);
  lastSyncTime = lastKpFetch;
  return true;
}

void ensureKpIndex() {
  time_t nowT = time(nullptr);
  if ((isnan(kpIndex) || (nowT - lastKpFetch) > KP_INTERVAL_SEC) &&
      WiFi.status() == WL_CONNECTED) {
    if (fetchKpIndex()) dataDirty = true;
  }
}

// =========================================================
// DRAW HELPERS
// =========================================================
void drawCard(int x, int y, int w, int h, bool accent = false) {
  tft.fillRoundRect(x, y, w, h, 8, COL_PANEL);
  tft.drawRoundRect(x, y, w, h, 8, accent ? COL_ACCENT : COL_STROKE);
}

void drawTopBar(const String& title) {
  tft.fillRect(0, 0, SCREEN_W, TOPBAR_H, COL_PANEL_ALT);
  tft.drawFastHLine(0, TOPBAR_H - 1, SCREEN_W, COL_STROKE);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_TEXT, COL_PANEL_ALT);
  tft.drawString(title, 10, 9, 2);

  const int bs = 25;
  const int bx = SCREEN_W - bs - 6;
  const int by = 4;

  uint16_t bg = (sleepDimmed || sleepOff) ? COL_ACCENT : COL_PANEL;
  uint16_t fg = (sleepDimmed || sleepOff) ? TFT_BLACK : COL_TEXT;

  tft.fillRoundRect(bx, by, bs, bs, 7, bg);
  tft.drawRoundRect(bx, by, bs, bs, 7, COL_ACCENT);

  const int cx = bx + 12;
  const int cy = by + 12;

  tft.drawCircle(cx, cy + 1, 6, fg);
  tft.drawFastHLine(cx - 4, cy - 5, 9, bg);
  tft.drawFastVLine(cx, cy - 7, 6, fg);
}

void drawNavBar() {
  const int y = SCREEN_H - NAV_H;
  tft.fillRect(0, y, SCREEN_W, NAV_H, COL_PANEL_ALT);
  tft.drawFastHLine(0, y, SCREEN_W, COL_STROKE);

  const int btnW = SCREEN_W / NAV_COUNT;
  const char* names[NAV_COUNT] = {"Home", "Wx", "Notes", "Status", "Setup"};

  for (int i = 0; i < NAV_COUNT; i++) {
    int bx = i * btnW;
    bool active = ((int)currentPage == i);

    uint16_t bg = active ? COL_ACCENT : COL_PANEL;
    uint16_t fg = active ? TFT_BLACK : COL_TEXT;

    tft.fillRoundRect(bx + 4, y + 6, btnW - 8, NAV_H - 12, 8, bg);
    tft.drawRoundRect(bx + 4, y + 6, btnW - 8, NAV_H - 12, 8, active ? COL_ACCENT : COL_STROKE);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(fg, bg);
    tft.drawString(names[i], bx + btnW / 2, y + NAV_H / 2, 1);
  }

  tft.setTextDatum(TL_DATUM);
}

void makeSpriteCard(TFT_eSprite& spr, int w, int h, bool accent = false) {
  spr.setColorDepth(16);
  spr.createSprite(w, h);
  spr.fillSprite(COL_BG);
  spr.fillRoundRect(0, 0, w, h, 10, COL_PANEL);
  spr.drawRoundRect(0, 0, w, h, 10, accent ? COL_ACCENT : COL_STROKE);
}

void pushSpriteAndDelete(TFT_eSprite& spr, int x, int y) {
  spr.pushSprite(x, y, COL_BG);
  spr.deleteSprite();
}

void drawCleanSunIcon(TFT_eSprite& spr, int cx, int cy, uint16_t c) {
  spr.fillCircle(cx, cy, 4, c);
  spr.drawLine(cx, cy - 9, cx, cy - 7, c);
  spr.drawLine(cx, cy + 7, cx, cy + 9, c);
  spr.drawLine(cx - 9, cy, cx - 7, cy, c);
  spr.drawLine(cx + 7, cy, cx + 9, cy, c);
  spr.drawLine(cx - 6, cy - 6, cx - 5, cy - 5, c);
  spr.drawLine(cx + 5, cy - 5, cx + 6, cy - 6, c);
  spr.drawLine(cx - 6, cy + 6, cx - 5, cy + 5, c);
  spr.drawLine(cx + 5, cy + 5, cx + 6, cy + 6, c);
}

void drawMoonIcon(TFT_eSprite& spr, int cx, int cy, uint16_t c) {
  spr.fillCircle(cx, cy, 6, c);
  spr.fillCircle(cx + 4, cy - 2, 6, COL_PANEL);
}

void drawWeatherSun(int cx, int cy, int r, uint16_t fg, uint16_t bg) {
  tft.fillCircle(cx, cy, r, fg);
  for (int i = 0; i < 8; i++) {
    float a = i * 0.785398f;
    int x1 = cx + (int)(cosf(a) * (r + 4));
    int y1 = cy + (int)(sinf(a) * (r + 4));
    int x2 = cx + (int)(cosf(a) * (r + 10));
    int y2 = cy + (int)(sinf(a) * (r + 10));
    tft.drawLine(x1, y1, x2, y2, fg);
  }
  tft.drawCircle(cx, cy, r + 1, bg);
}

void drawWeatherCloud(int cx, int cy, int scale, uint16_t fg, uint16_t bg) {
  int r = scale;
  tft.fillCircle(cx - r, cy + 2, r, fg);
  tft.fillCircle(cx + 2, cy - 3, r + 3, fg);
  tft.fillCircle(cx + r + 5, cy + 3, r - 1, fg);
  tft.fillRoundRect(cx - r * 2, cy + 2, r * 4 + 8, r + 9, 5, fg);
  tft.drawFastHLine(cx - r * 2, cy + r + 11, r * 4 + 8, bg);
}

void drawWeatherRain(int cx, int cy, int scale, uint16_t fg) {
  int len = max(4, scale + 2);
  for (int i = -1; i <= 1; i++) {
    int x = cx + i * scale;
    tft.drawLine(x, cy, x - max(1, scale / 2), cy + len, fg);
  }
}

void drawWeatherSnow(int cx, int cy, int scale, uint16_t fg) {
  int len = max(2, scale / 2);
  for (int i = -1; i <= 1; i++) {
    int x = cx + i * scale;
    tft.drawLine(x - len, cy + len, x + len, cy + len * 2, fg);
    tft.drawLine(x + len, cy + len, x - len, cy + len * 2, fg);
  }
}

void drawWeatherIcon(int cx, int cy, int code, int size, uint16_t bg) {
  uint16_t sun = COL_YELLOW;
  uint16_t cloud = COL_TEXT;
  uint16_t rain = COL_ACCENT;
  int r = max(2, size / 5);
  if (code < 0) {
    tft.drawCircle(cx, cy, r + 6, COL_DIM);
    tft.drawLine(cx - 5, cy, cx + 5, cy, COL_DIM);
    return;
  }
  if (code == 0) {
    drawWeatherSun(cx, cy, r + 2, sun, bg);
    return;
  }
  if (code <= 3) {
    if (code <= 2) drawWeatherSun(cx - r, cy - r, r, sun, bg);
    drawWeatherCloud(cx + 1, cy + 1, r, cloud, bg);
    return;
  }
  if (code == 45 || code == 48) {
    drawWeatherCloud(cx, cy - 2, r, cloud, bg);
    tft.drawFastHLine(cx - size / 2, cy + r + 9, size, COL_DIM);
    tft.drawFastHLine(cx - size / 3, cy + r + 15, size * 2 / 3, COL_DIM);
    return;
  }
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
    drawWeatherCloud(cx, cy - 3, r, cloud, bg);
    drawWeatherRain(cx, cy + r + 9, r + 3, rain);
    return;
  }
  if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86)) {
    drawWeatherCloud(cx, cy - 3, r, cloud, bg);
    drawWeatherSnow(cx, cy + r + 7, r + 3, rain);
    return;
  }
  if (code >= 95) {
    drawWeatherCloud(cx, cy - 5, r, cloud, bg);
    tft.fillTriangle(cx - 2, cy + r + 5, cx + 8, cy + r + 5, cx, cy + r + 18, COL_YELLOW);
    tft.fillTriangle(cx, cy + r + 13, cx + 9, cy + r + 13, cx - 2, cy + r + 28, COL_YELLOW);
    return;
  }
  drawWeatherCloud(cx, cy, r, cloud, bg);
}

int drawWrappedTextLimited(int x, int y, int maxW, const String& text, int font, uint16_t fg, uint16_t bg, int maxLines) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(fg, bg);

  const int lineH = tft.fontHeight(font) + 2;
  String line = "";
  String word = "";
  int linesDrawn = 0;

  auto flushLine = [&]() {
    if (linesDrawn >= maxLines) return;
    if (line.length() > 0) tft.drawString(line, x, y, font);
    y += lineH;
    line = "";
    linesDrawn++;
  };

  auto placeWordOnEmptyLine = [&]() {
    if (word.length() == 0 || linesDrawn >= maxLines) return;

    while (tft.textWidth(word, font) > maxW && word.length() > 1) {
      int cut = word.length();
      while (cut > 1 && tft.textWidth(word.substring(0, cut), font) > maxW) cut--;
      if (linesDrawn >= maxLines) return;
      tft.drawString(word.substring(0, cut), x, y, font);
      y += lineH;
      linesDrawn++;
      word = word.substring(cut);
    }

    if (linesDrawn < maxLines) {
      line = word;
      word = "";
    }
  };

  auto flushWord = [&]() {
    if (word.length() == 0 || linesDrawn >= maxLines) return;

    if (line.length() == 0) {
      placeWordOnEmptyLine();
      return;
    }

    String candidate = line + " " + word;
    if (tft.textWidth(candidate, font) <= maxW) {
      line = candidate;
      word = "";
      return;
    }

    flushLine();
    placeWordOnEmptyLine();
  };

  for (int i = 0; i < (int)text.length(); i++) {
    if (linesDrawn >= maxLines) break;
    char c = text[i];

    if (c == '\n') {
      flushWord();
      flushLine();
      continue;
    }

    if (c == ' ') {
      flushWord();
      continue;
    }

    word += c;
  }

  if (linesDrawn < maxLines) {
    flushWord();
    if (line.length() > 0) flushLine();
  }

  return y;
}

// =========================================================
// HOME SPRITES
// =========================================================
void drawClockCardSprite(bool force = false) {
  const int x = 8, y = PAGE_ROW1_Y, w = 224, h = HOME_WIDGET_H;

  time_t now = time(nullptr);
  struct tm tmNow;
  localtime_r(&now, &tmNow);

  String timeBuf = formatClockParts(tmNow, true);
  String dateBuf = formatDateParts(tmNow);

  String sr = formatMinuteOfDay(sunriseMin);
  String ss = formatMinuteOfDay(sunsetMin);

  String combined = timeBuf + "|" + dateBuf + "|" + sr + "|" + ss + "|" +
                    String(COL_ACCENT) + "|" + String(COL_TEXT);

  if (!force && combined == cacheClock) return;
  cacheClock = combined;

  makeSpriteCard(sprClock, w, h, true);

  sprClock.setTextDatum(TL_DATUM);

  sprClock.setTextColor(COL_TEXT, COL_PANEL);
  if (useUsRegionFormat()) {
    int splitAt = timeBuf.lastIndexOf(' ');
    String clockMain = splitAt > 0 ? timeBuf.substring(0, splitAt) : timeBuf;
    String clockSuffix = splitAt > 0 ? timeBuf.substring(splitAt + 1) : "";
    sprClock.drawString(clockMain, 10, 11, 4);
    if (clockSuffix.length() > 0) {
      int suffixX = 10 + sprClock.textWidth(clockMain, 4) + 4;
      sprClock.drawString(clockSuffix, suffixX, 18, 2);
    }
  } else {
    sprClock.drawString(timeBuf, 10, 11, 4);
  }

  sprClock.setTextColor(COL_DIM, COL_PANEL);
  sprClock.drawString(dateBuf, 10, 45, 2);

  drawCleanSunIcon(sprClock, 151, 22, COL_ACCENT);
  drawMoonIcon(sprClock, 151, 50, COL_ACCENT);

  sprClock.setTextColor(COL_ACCENT, COL_PANEL);
  sprClock.drawString(sr, 165, 15, 2);
  sprClock.drawString(ss, 165, 43, 2);

  pushSpriteAndDelete(sprClock, x, y);
}

void drawMetricSprite(int x, int y, int w, int h, const char* label, const String& value, String& cache, bool force = false, const String& detail = "") {
  String combined = String(label) + "|" + value + "|" + detail + "|" + String(COL_PANEL) + "|" +
                    String(COL_STROKE) + "|" + String(COL_TEXT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString(label, 10, 8, 2);

  sprSmall.setTextColor(COL_TEXT, COL_PANEL);
  sprSmall.drawString(value, 10, 31, 4);

  if (detail.length() > 0) {
    sprSmall.setTextColor(COL_ACCENT, COL_PANEL);
    sprSmall.drawString(detail, 10, 55, 1);
  }

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawWeatherStyleMetricSprite(int x, int y, int w, int h, const char* label, const String& value, String& cache, bool force = false, const String& detail = "") {
  String combined = String(label) + "|" + value + "|" + detail + "|" + String(COL_PANEL) + "|" +
                    String(COL_STROKE) + "|" + String(COL_TEXT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString(label, 10, 8, 2);

  sprSmall.setTextColor(COL_TEXT, COL_PANEL);
  sprSmall.drawString(value, 10, 28, 4);

  if (detail.length() > 0) {
    sprSmall.setTextColor(COL_ACCENT, COL_PANEL);
    sprSmall.drawString(detail, 10, 52, 1);
  }

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawSunEventWidget(int x, int y, int w, int h, String& cache, bool force = false) {
  String label = nextSunLabel();
  String value = nextSunTimeText();
  String combined = label + "|" + value + "|" + String(COL_PANEL) + "|" +
                    String(COL_STROKE) + "|" + String(COL_TEXT) + "|" + String(COL_ACCENT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString(label, 10, 8, 2);

  sprSmall.setTextColor(COL_TEXT, COL_PANEL);
  if (useUsRegionFormat()) {
    int splitAt = value.lastIndexOf(' ');
    String mainValue = splitAt > 0 ? value.substring(0, splitAt) : value;
    String suffix = splitAt > 0 ? value.substring(splitAt + 1) : "";
    sprSmall.drawString(mainValue, 10, 30, 4);
    if (suffix.length() > 0) {
      int suffixX = 10 + sprSmall.textWidth(mainValue, 4) + 3;
      sprSmall.drawString(suffix, suffixX, 35, 2);
    }
  } else {
    sprSmall.drawString(value, 10, 30, 4);
  }

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawPlaceholderSprite(int x, int y, int w, int h, const char* label, String& cache, bool force = false) {
  String combined = String(label) + "|" + String(COL_PANEL) + "|" + String(COL_STROKE) + "|" + String(COL_TEXT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString(label, 10, 8, 2);

  sprSmall.setTextColor(COL_STROKE, COL_PANEL);
  sprSmall.drawString("Empty", 10, 31, 2);

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawFocusTimerWidget(int x, int y, int w, int h, String& cache, bool force = false) {
  String value = formatTimerClock(focusRemainingSec);
  String hint = focusHintText();
  String combined = value + "|" + hint + "|" + String(focusMenuOpen ? 1 : 0) +
                    "|" + String(COL_PANEL) + "|" + String(COL_ACCENT) + "|" + String(COL_TEXT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString("Timer", 10, 8, 2);

  if (focusMenuOpen) {
    sprSmall.setTextColor(COL_TEXT, COL_PANEL);
    sprSmall.drawString("Select", 10, 22, 4);
    sprSmall.setTextColor(COL_DIM, COL_PANEL);
    sprSmall.drawString("duration", 10, 44, 2);
  } else {
    sprSmall.setTextColor(focusTimerFinished ? COL_GREEN : COL_TEXT, COL_PANEL);
    sprSmall.drawString(value, 10, 24, 4);
    sprSmall.setTextColor(COL_DIM, COL_PANEL);
    sprSmall.drawString(hint, 10, 49, 1);
  }

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawHomeSlotWidget(int slot, bool force = false) {
  int x, y, w, h;
  getHomeSlotRect(slot, x, y, w, h);

  switch (homeWidgetSlots[slot]) {
    case HOME_WIDGET_WEEK:
      drawMetricSprite(x, y, w, h, "Week", weekNumberText(), cacheHomeSlots[slot], force);
      break;
    case HOME_WIDGET_TIMER:
      drawFocusTimerWidget(x, y, w, h, cacheHomeSlots[slot], force);
      break;
    case HOME_WIDGET_RAIN:
      drawWeatherStyleMetricSprite(x, y, w, h, "Rain", rainText(), cacheHomeSlots[slot], force);
      break;
    case HOME_WIDGET_OUTDOOR:
      drawWeatherStyleMetricSprite(x, y, w, h, "Outdoor", tempText(), cacheHomeSlots[slot], force, tempRangeText());
      break;
    case HOME_WIDGET_KP:
      drawWeatherStyleMetricSprite(x, y, w, h, "KP index", kpText(), cacheHomeSlots[slot], force, kpLevelText());
      break;
    case HOME_WIDGET_UV:
      drawWeatherStyleMetricSprite(x, y, w, h, "UV index", uvText(), cacheHomeSlots[slot], force, uvLevelText());
      break;
    case HOME_WIDGET_WIND:
      drawWeatherStyleMetricSprite(x, y, w, h, "Wind", windText(), cacheHomeSlots[slot], force, windDirectionText());
      break;
    case HOME_WIDGET_SUN:
      drawSunEventWidget(x, y, w, h, cacheHomeSlots[slot], force);
      break;
  }
}

void drawHomeHero(bool force = false) {
  time_t now = time(nullptr);
  struct tm tmNow;
  localtime_r(&now, &tmNow);

  String timeBuf = formatClockParts(tmNow, false);
  String dateBuf = formatDateParts(tmNow);
  String condition = weatherConditionText(weatherCode);
  String combined = timeBuf + "|" + dateBuf + "|" + tempText() + "|" + tempRangeText() + "|" +
                    condition + "|" + locationName + "|" + wifiStatusText() + "|" +
                    String(COL_PANEL) + "|" + String(COL_TEXT) + "|" + String(COL_ACCENT);
  if (!force && combined == cacheHomeHero) return;
  cacheHomeHero = combined;

  drawCard(8, 40, 224, 82, true);
  tft.fillRect(16, 48, 208, 66, COL_PANEL);

  tft.setTextColor(COL_TEXT, COL_PANEL);
  if (useUsRegionFormat()) {
    int splitAt = timeBuf.lastIndexOf(' ');
    String mainTime = splitAt > 0 ? timeBuf.substring(0, splitAt) : timeBuf;
    String suffix = splitAt > 0 ? timeBuf.substring(splitAt + 1) : "";
    tft.drawString(mainTime, 18, 52, 4);
    if (suffix.length() > 0) tft.drawString(suffix, 84, 59, 2);
  } else {
    tft.drawString(timeBuf, 18, 52, 4);
  }

  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString(dateBuf, 18, 84, 1);
  tft.drawString(wifiStatusText(), 18, 101, 1);

  tft.setTextColor(COL_TEXT, COL_PANEL);
  tft.drawRightString(tempText().c_str(), 222, 52, 4);
  tft.setTextColor(COL_ACCENT, COL_PANEL);
  tft.drawRightString(condition.substring(0, 10).c_str(), 222, 84, 1);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawRightString(locationName.substring(0, 12).c_str(), 222, 101, 1);
}

void drawHomeFocusCard(bool force = false) {
  String value = formatTimerClock(focusRemainingSec);
  String hint = focusHintText();
  String combined = value + "|" + hint + "|" + String(focusTimerRunning ? 1 : 0) + "|" +
                    String(focusTimerFinished ? 1 : 0) + "|" + String(COL_PANEL);
  if (!force && combined == cacheHomeFocus) return;
  cacheHomeFocus = combined;

  drawCard(8, 128, 108, 58, true);
  tft.fillRect(16, 136, 92, 42, COL_PANEL);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString("Focus", 18, 137, 1);
  tft.setTextColor(focusTimerFinished ? COL_GREEN : COL_TEXT, COL_PANEL);
  tft.drawString(value, 18, 151, 4);
  tft.setTextColor(COL_ACCENT, COL_PANEL);
  tft.drawRightString(hint.substring(0, 12).c_str(), 108, 174, 1);
}

void drawHomeForecastCard(bool force = false) {
  String nextTemp = hourlyForecasts[1].valid ? formatDisplayTemp(hourlyForecasts[1].tempC) : tempRangeText();
  String pop = hourlyForecasts[1].valid && !isnan(hourlyForecasts[1].precipProb)
    ? String((int)roundf(hourlyForecasts[1].precipProb)) + "%"
    : rainText();
  String combined = nextTemp + "|" + pop + "|" + windText() + "|" + uvText() + "|" + String(COL_PANEL);
  if (!force && combined == cacheHomeForecast) return;
  cacheHomeForecast = combined;

  drawCard(124, 128, 108, 58, true);
  tft.fillRect(132, 136, 92, 42, COL_PANEL);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString("Next hour", 134, 137, 1);
  tft.setTextColor(COL_TEXT, COL_PANEL);
  tft.drawString(nextTemp, 134, 151, 4);
  tft.setTextColor(COL_ACCENT, COL_PANEL);
  tft.drawRightString(("Rain " + pop).c_str(), 224, 174, 1);
}

void drawHomeTicker(bool force = false) {
  String ticker = homeTickerText();
  String combined = ticker + "|" + insightStatus + "|" + String(COL_PANEL) + "|" + String(COL_TEXT) + "|" + String(COL_ACCENT);
  if (force || combined != cacheHomeTicker) {
    cacheHomeTicker = combined;
    homeTickerOffset = 0;
    drawCard(8, 194, 224, 36, true);
  }

  sprSmall.setColorDepth(16);
  sprSmall.createSprite(208, 20);
  sprSmall.fillSprite(COL_PANEL);
  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_ACCENT, COL_PANEL);
  sprSmall.drawString("Live", 0, 2, 1);
  sprSmall.setTextColor(COL_TEXT, COL_PANEL);

  int textX = 34 - homeTickerOffset;
  int textW = sprSmall.textWidth(ticker, 2);
  sprSmall.drawString(ticker, textX, 1, 2);
  if (textX + textW < 208) sprSmall.drawString(ticker, textX + textW + 44, 1, 2);
  sprSmall.pushSprite(16, 202);
  sprSmall.deleteSprite();

  int loopW = textW + 44;
  homeTickerOffset = loopW > 0 ? (homeTickerOffset + 4) % loopW : 0;
}

void drawHomeNoteCard(bool force = false) {
  String note = compactNoteText();
  String combined = note + "|" + nextSunLabel() + "|" + nextSunTimeText() + "|" + String(COL_PANEL);
  if (!force && combined == cacheHomeNote) return;
  cacheHomeNote = combined;

  drawCard(8, 236, 224, 36, false);
  tft.fillRect(16, 244, 208, 20, COL_PANEL);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString("Note", 18, 245, 1);
  tft.setTextColor(COL_TEXT, COL_PANEL);
  tft.drawString(note.substring(0, 24), 52, 245, 1);
  tft.setTextColor(COL_ACCENT, COL_PANEL);
  tft.drawRightString((nextSunLabel() + " " + nextSunTimeText()).c_str(), 224, 259, 1);
}

void drawShowDots() {
  const int y = 262;
  const int startX = 100;
  for (int i = 0; i < HOME_SHOW_SLIDE_COUNT; i++) {
    tft.fillCircle(startX + i * 13, y, 3, i == homeShowSlide ? COL_ACCENT : COL_STROKE);
  }
}

void drawShowCardTitle(const String& title, const String& subtitle = "") {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_DIM, COL_BG);
  tft.drawString(title, 12, 41, 2);
  if (subtitle.length() > 0) {
    tft.setTextColor(COL_ACCENT, COL_BG);
    tft.drawRightString(subtitle.substring(0, 18).c_str(), 228, 42, 1);
  }
}

void drawHomeOverviewSlide() {
  time_t now = time(nullptr);
  struct tm tmNow;
  localtime_r(&now, &tmNow);
  String timeBuf = formatClockParts(tmNow, false);

  drawShowCardTitle(homeTitleText(), wifiStatusText());
  drawCard(8, 62, 224, 94, true);
  tft.fillRect(18, 72, 204, 72, COL_PANEL);
  tft.setTextColor(COL_TEXT, COL_PANEL);
  tft.drawString(timeBuf, 20, 76, 4);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString(formatDateParts(tmNow), 20, 110, 2);
  tft.setTextColor(COL_TEXT, COL_PANEL);
  tft.drawRightString(tempText().c_str(), 222, 78, 4);
  tft.setTextColor(COL_ACCENT, COL_PANEL);
  tft.drawRightString(weatherConditionText(weatherCode).substring(0, 12).c_str(), 222, 112, 1);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawRightString(locationName.substring(0, 14).c_str(), 222, 130, 1);

  drawCard(8, 166, 108, 76, true);
  tft.fillRect(18, 176, 88, 54, COL_PANEL);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString("Focus", 18, 176, 2);
  tft.setTextColor(focusTimerFinished ? COL_GREEN : COL_TEXT, COL_PANEL);
  tft.drawString(formatTimerClock(focusRemainingSec), 18, 198, 4);
  tft.setTextColor(COL_ACCENT, COL_PANEL);
  tft.drawString(focusHintText().substring(0, 14), 18, 226, 1);

  drawCard(124, 166, 108, 76, true);
  tft.fillRect(134, 176, 88, 54, COL_PANEL);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString("Next", 134, 176, 2);
  String nextTemp = hourlyForecasts[1].valid ? formatDisplayTemp(hourlyForecasts[1].tempC) : tempRangeText();
  tft.setTextColor(COL_TEXT, COL_PANEL);
  tft.drawString(nextTemp, 134, 198, 4);
  tft.setTextColor(COL_ACCENT, COL_PANEL);
  String pop = hourlyForecasts[1].valid && !isnan(hourlyForecasts[1].precipProb) ? String((int)roundf(hourlyForecasts[1].precipProb)) + "% rain" : rainText();
  tft.drawString(pop.substring(0, 14), 134, 226, 1);
}

void drawHomeWeatherSlide() {
  drawShowCardTitle("Weather", lastSyncText());
  drawCard(8, 62, 224, 82, true);
  tft.fillRect(18, 72, 204, 58, COL_PANEL);
  drawWeatherIcon(184, 97, weatherCode, 48, COL_PANEL);
  tft.setTextColor(COL_TEXT, COL_PANEL);
  tft.drawString(tempText(), 20, 74, 4);
  tft.setTextColor(COL_ACCENT, COL_PANEL);
  tft.drawString(weatherConditionText(weatherCode).substring(0, 18), 20, 108, 2);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString(tempRangeText(), 20, 128, 1);
  tft.drawRightString((windText() + "  " + uvText()).c_str(), 222, 128, 1);

  drawCard(8, 152, 224, 46, true);
  tft.fillRect(18, 160, 204, 28, COL_PANEL);
  for (int i = 0; i < 4; i++) {
    int x = 20 + i * 50;
    if (!hourlyForecasts[i].valid) continue;
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawCentreString(formatHourLabel(hourlyForecasts[i].minuteOfDay).c_str(), x + 18, 160, 1);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawCentreString(formatCompactTemp(hourlyForecasts[i].tempC).c_str(), x + 18, 174, 2);
  }

  drawCard(8, 206, 224, 48, false);
  tft.fillRect(18, 214, 204, 30, COL_PANEL);
  for (int i = 0; i < 3; i++) {
    if (!dailyForecasts[i].valid) continue;
    int x = 18 + i * 68;
    tft.setTextColor(i == 0 ? COL_TEXT : COL_DIM, COL_PANEL);
    tft.drawString(dailyForecasts[i].label.substring(0, 5), x, 214, 1);
    tft.drawString(weatherShortText(dailyForecasts[i].weatherCode).substring(0, 6), x, 222, 1);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    String range = formatCompactTemp(dailyForecasts[i].maxC) + "/" + formatCompactTemp(dailyForecasts[i].minC);
    tft.drawString(range, x, 236, 1);
  }
}

void drawHomeFocusSlide() {
  drawShowCardTitle("Focus and checklist", "Tap to check");
  drawCard(8, 62, 224, 70, true);
  tft.fillRect(18, 72, 204, 48, COL_PANEL);
  tft.setTextColor(focusTimerFinished ? COL_GREEN : COL_TEXT, COL_PANEL);
  tft.drawString(formatTimerClock(focusRemainingSec), 20, 76, 4);
  tft.setTextColor(COL_ACCENT, COL_PANEL);
  tft.drawRightString(focusHintText().substring(0, 18).c_str(), 222, 102, 2);

  drawCard(8, 142, 224, 112, true);
  tft.fillRect(18, 152, 204, 90, COL_PANEL);
  for (int i = 0; i < CHECKLIST_COUNT; i++) {
    int y = 154 + i * 22;
    uint16_t boxColor = checklistDone[i] ? COL_ACCENT : COL_STROKE;
    tft.drawRoundRect(20, y, 14, 14, 3, boxColor);
    if (checklistDone[i]) {
      tft.drawLine(23, y + 7, 27, y + 11, COL_ACCENT);
      tft.drawLine(27, y + 11, 33, y + 3, COL_ACCENT);
    }
    tft.setTextColor(checklistDone[i] ? COL_DIM : COL_TEXT, COL_PANEL);
    tft.drawString(checklistItems[i].substring(0, 29), 42, y - 1, 2);
  }
}

void drawHomeLiveSlide() {
  drawShowCardTitle("Briefing", insightSource.substring(0, 18));
  drawCard(8, 62, 224, 142, true);
  tft.fillRect(18, 72, 204, 120, COL_PANEL);
  tft.setTextColor(COL_ACCENT, COL_PANEL);
  tft.drawString(insightTitle.substring(0, 26), 20, 76, 2);
  drawWrappedTextLimited(20, 104, 198, insightBody, 2, COL_TEXT, COL_PANEL, 5);

  drawCard(8, 214, 224, 40, false);
  tft.fillRect(18, 222, 204, 22, COL_PANEL);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString("Status", 20, 224, 1);
  tft.setTextColor(COL_TEXT, COL_PANEL);
  tft.drawRightString(insightStatus.substring(0, 24).c_str(), 222, 224, 1);
  tft.setTextColor(COL_ACCENT, COL_PANEL);
  tft.drawString(compactNoteText().substring(0, 34), 20, 240, 1);
}

void drawHomeBibleSlide() {
  DailyReading reading = dailyReading();
  drawShowCardTitle("Daily reading", reading.ref);
  drawCard(8, 62, 224, 142, true);
  tft.fillRect(18, 72, 204, 120, COL_PANEL);
  tft.setTextColor(COL_ACCENT, COL_PANEL);
  tft.drawString("Psalm of the day", 20, 76, 2);
  tft.setTextColor(COL_TEXT, COL_PANEL);
  tft.drawString(reading.ref, 20, 101, 4);
  drawWrappedTextLimited(20, 138, 198, String(reading.text), 2, COL_TEXT, COL_PANEL, 4);

  drawCard(8, 214, 224, 40, false);
  tft.fillRect(18, 222, 204, 22, COL_PANEL);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString("Pause", 20, 224, 1);
  tft.setTextColor(COL_ACCENT, COL_PANEL);
  tft.drawString(String(reading.prompt).substring(0, 34), 20, 240, 1);
}

void drawHomeShowSlide(bool force = false) {
  DailyReading reading = dailyReading();
  String key = String(homeShowSlide) + "|" + tempText() + "|" + weatherConditionText(weatherCode) + "|" +
               formatTimerClock(focusRemainingSec) + "|" + focusHintText() + "|" + insightTitle + "|" +
               insightBody + "|" + insightStatus + "|" + compactNoteText() + "|" + reading.ref + "|" + reading.text;
  for (int i = 0; i < CHECKLIST_COUNT; i++) {
    key += "|" + String(checklistDone[i] ? 1 : 0) + checklistItems[i];
  }
  if (!force && key == cacheHomeShow) return;
  cacheHomeShow = key;
  tft.fillRect(0, TOPBAR_H, SCREEN_W, SCREEN_H - TOPBAR_H - NAV_H, COL_BG);
  switch (homeShowSlide) {
    case 0: drawHomeOverviewSlide(); break;
    case 1: drawHomeWeatherSlide(); break;
    case 2: drawHomeFocusSlide(); break;
    case 3: drawHomeLiveSlide(); break;
    case 4: drawHomeBibleSlide(); break;
  }
  drawShowDots();
}

void advanceHomeShowSlide() {
  homeShowSlide = (homeShowSlide + 1) % HOME_SHOW_SLIDE_COUNT;
  lastHomeShowSlideMs = millis();
  cacheHomeShow = "";
  pageDirty = true;
}

void drawFocusMenuOverlay(bool force = false) {
  String combined = String(focusTimerRunning ? 1 : 0) + "|" + String(focusTimerFinished ? 1 : 0) +
                    "|" + String(COL_PANEL_ALT) + "|" + String(COL_PANEL) + "|" + String(COL_ACCENT);
  if (!force && combined == cacheTimerMenu) return;
  cacheTimerMenu = combined;

  tft.fillRect(0, 0, SCREEN_W, SCREEN_H, COL_BG);
  tft.fillRoundRect(TIMER_MENU_X, TIMER_MENU_Y, TIMER_MENU_W, TIMER_MENU_H, 12, COL_PANEL_ALT);
  tft.drawRoundRect(TIMER_MENU_X, TIMER_MENU_Y, TIMER_MENU_W, TIMER_MENU_H, 12, COL_ACCENT);

  tft.setTextColor(COL_TEXT, COL_PANEL_ALT);
  tft.drawString("Start timer", TIMER_MENU_X + 14, TIMER_MENU_Y + 12, 2);
  tft.setTextColor(COL_DIM, COL_PANEL_ALT);
  tft.drawString("Choose a session length", TIMER_MENU_X + 14, TIMER_MENU_Y + 34, 1);

  const int btnW = 74;
  const int btnH = 28;
  const int col1X = TIMER_MENU_X + 14;
  const int col2X = TIMER_MENU_X + 112;
  const int row1Y = TIMER_MENU_Y + 54;
  const int row2Y = TIMER_MENU_Y + 88;
  const int row3Y = TIMER_MENU_Y + 122;

  String labels[6];
  const int xs[] = {col1X, col2X, col1X, col2X, col1X, col2X};
  const int ys[] = {row1Y, row1Y, row2Y, row2Y, row3Y, row3Y};
  for (int i = 0; i < 6; i++) {
    labels[i] = String(timerPresetMin[i]) + " min";
  }

  for (int i = 0; i < 6; i++) {
    tft.fillRoundRect(xs[i], ys[i], btnW, btnH, 8, COL_PANEL);
    tft.drawRoundRect(xs[i], ys[i], btnW, btnH, 8, COL_STROKE);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawCentreString(labels[i].c_str(), xs[i] + btnW / 2, ys[i] + 7, 2);
  }

  const int actionY = TIMER_MENU_Y + 160;
  const int actionW = 152;
  const int actionX = TIMER_MENU_X + 24;
  const char* actionLabel = focusTimerRunning ? "Stop" : (focusTimerFinished ? "Reset" : nullptr);
  uint16_t actionColor = focusTimerRunning ? COL_RED : COL_ACCENT;

  if (actionLabel) {
    tft.fillRoundRect(actionX, actionY - 4, actionW, 26, 8, COL_PANEL);
    tft.drawRoundRect(actionX, actionY - 4, actionW, 26, 8, actionColor);
    tft.setTextColor(actionColor, COL_PANEL);
    tft.drawCentreString(actionLabel, actionX + actionW / 2, actionY + 4, 2);
  } else {
    tft.setTextColor(COL_DIM, COL_PANEL_ALT);
    tft.drawCentreString("Tap outside to close", TIMER_MENU_X + TIMER_MENU_W / 2, TIMER_MENU_Y + TIMER_MENU_H - 13, 1);
  }
}

void drawTimerDoneOverlay(bool force = false) {
  if (!timerDoneDialogOpen) return;

  String elapsed = formatElapsedText(focusDurationSec);
  String countdown = timerDoneCountdownText();
  bool flashOn = flashModeEnabled && ((millis() / 300UL) % 2UL == 0);
  setBacklight(flashModeEnabled ? (flashOn ? FLASH_BL_HIGH : FLASH_BL_LOW) : BL_FULL);
  String combined = elapsed + "|" + String(COL_PANEL_ALT) + "|" + String(COL_ACCENT) + "|" + String(COL_TEXT);
  String flashKey = String(flashOn ? 1 : 0);
  if (force || combined != cacheTimerDone || flashKey != cacheTimerDoneFlash) {
    cacheTimerDone = combined;
    cacheTimerDoneFlash = flashKey;
    cacheTimerDoneCountdown = "";

    uint16_t backdrop = flashOn ? COL_ACCENT : COL_BG;
    uint16_t panelBorder = flashOn ? TFT_WHITE : COL_ACCENT;
    tft.fillRect(0, 0, SCREEN_W, SCREEN_H, backdrop);
    tft.fillRoundRect(TIMER_DONE_X, TIMER_DONE_Y, TIMER_DONE_W, TIMER_DONE_H, 12, COL_PANEL_ALT);
    tft.drawRoundRect(TIMER_DONE_X, TIMER_DONE_Y, TIMER_DONE_W, TIMER_DONE_H, 12, panelBorder);

    tft.setTextColor(COL_TEXT, COL_PANEL_ALT);
    tft.drawCentreString("Timer complete", TIMER_DONE_X + TIMER_DONE_W / 2, TIMER_DONE_Y + 14, 2);
    tft.setTextColor(COL_ACCENT, COL_PANEL_ALT);
    tft.drawCentreString(elapsed, TIMER_DONE_X + TIMER_DONE_W / 2, TIMER_DONE_Y + 42, 2);
    tft.setTextColor(COL_DIM, COL_PANEL_ALT);
    tft.drawCentreString("Tap anywhere to acknowledge", TIMER_DONE_X + TIMER_DONE_W / 2, TIMER_DONE_Y + 68, 1);
  }

  if (force || countdown != cacheTimerDoneCountdown) {
    cacheTimerDoneCountdown = countdown;
    tft.fillRect(TIMER_DONE_X + 28, TIMER_DONE_Y + 82, TIMER_DONE_W - 56, 12, COL_PANEL_ALT);
    tft.setTextColor(COL_DIM, COL_PANEL_ALT);
    tft.drawCentreString(countdown.c_str(), TIMER_DONE_X + TIMER_DONE_W / 2, TIMER_DONE_Y + 82, 1);
  }
}

// =========================================================
// PAGES
// =========================================================
void drawHomePageFull() {
  tft.fillScreen(COL_BG);
  drawTopBar(homeTitleText());
  drawNavBar();

  cacheClock = "";
  cacheHomeEmpty1 = "";
  cacheHomeEmpty2 = "";
  cacheFocusTimer = "";
  cacheHomeHero = "";
  cacheHomeFocus = "";
  cacheHomeForecast = "";
  cacheHomeNote = "";
  cacheHomeTicker = "";
  cacheHomeShow = "";
  homeTickerOffset = 0;
  lastHomeShowSlideMs = millis();
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    cacheHomeSlots[i] = "";
  }

  pageDirty = false;
  lastDrawnPage = PAGE_HOME;

  drawHomeShowSlide(true);
  if (focusMenuOpen) drawFocusMenuOverlay(true);
  if (timerDoneDialogOpen) drawTimerDoneOverlay(true);
}

void updateHomeDynamic() {
  if (timerDoneDialogOpen) {
    drawTimerDoneOverlay(false);
    return;
  }

  if (focusMenuOpen) {
    drawFocusMenuOverlay(false);
    return;
  }

  drawHomeShowSlide(false);
}

void drawWeatherPageFull() {
  tft.fillScreen(COL_BG);
  drawTopBar("Weather");
  drawNavBar();

  drawCard(8, 40, 224, 78, true);
  drawCard(8, 124, 224, 64, true);
  drawCard(8, 194, 224, 78, true);

  pageDirty = false;
  lastDrawnPage = PAGE_WEATHER;

  lastWeatherPanelText = "";
}

void updateWeatherDynamic() {
  String key = tempText() + "|" + tempRangeText() + "|" + rainText() + "|" + windText() + "|" +
               uvText() + "|" + String(weatherCode) + "|" + locationName + "|" + lastSyncText();
  for (int i = 0; i < HOURLY_FORECAST_COUNT; i++) {
    key += "|" + String(hourlyForecasts[i].valid ? 1 : 0) + ":" +
           String(hourlyForecasts[i].minuteOfDay) + ":" +
           String(hourlyForecasts[i].tempC, 1) + ":" +
           String(hourlyForecasts[i].precipProb, 0) + ":" +
           String(hourlyForecasts[i].weatherCode);
  }
  for (int i = 0; i < DAILY_FORECAST_COUNT; i++) {
    key += "|" + String(dailyForecasts[i].valid ? 1 : 0) + ":" +
           dailyForecasts[i].label + ":" +
           String(dailyForecasts[i].minC, 1) + ":" +
           String(dailyForecasts[i].maxC, 1) + ":" +
           String(dailyForecasts[i].precipProb, 0) + ":" +
           String(dailyForecasts[i].weatherCode);
  }
  if (!dataDirty && key == lastWeatherPanelText) return;
  lastWeatherPanelText = key;

  tft.fillRect(12, 44, 216, 70, COL_PANEL);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString(locationName, 18, 48, 2);
  drawWeatherIcon(108, 80, weatherCode, 28, COL_PANEL);
  tft.drawString(weatherConditionText(weatherCode).substring(0, 10), 18, 72, 2);

  tft.setTextColor(COL_TEXT, COL_PANEL);
  tft.drawString(tempText(), 132, 50, 4);
  tft.setTextColor(COL_ACCENT, COL_PANEL);
  tft.drawString(tempRangeText(), 18, 94, 1);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString(windText() + "  " + uvText(), 118, 94, 1);

  tft.fillRect(12, 128, 216, 56, COL_PANEL);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString("Next hours", 18, 130, 1);
  const int colW = 34;
  for (int i = 0; i < HOURLY_FORECAST_COUNT; i++) {
    int x = 18 + i * colW;
    if (i < HOURLY_FORECAST_COUNT - 1) tft.drawFastVLine(x + colW - 4, 145, 30, COL_STROKE);
    if (!hourlyForecasts[i].valid) {
      tft.setTextColor(COL_DIM, COL_PANEL);
      tft.drawCentreString("--", x + 14, 158, 1);
      continue;
    }
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawCentreString(formatHourLabel(hourlyForecasts[i].minuteOfDay).c_str(), x + 14, 146, 1);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawCentreString(formatCompactTemp(hourlyForecasts[i].tempC).c_str(), x + 14, 158, 2);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    String pop = isnan(hourlyForecasts[i].precipProb) ? "--" : String((int)roundf(hourlyForecasts[i].precipProb)) + "%";
    tft.drawCentreString(pop.c_str(), x + 14, 176, 1);
  }

  tft.fillRect(12, 198, 216, 70, COL_PANEL);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString("7-day forecast", 18, 199, 1);
  for (int i = 0; i < DAILY_FORECAST_COUNT; i++) {
    int y = 211 + i * 8;
    if (!dailyForecasts[i].valid) continue;
    tft.setTextColor(i == 0 ? COL_TEXT : COL_DIM, COL_PANEL);
    tft.drawString(dailyForecasts[i].label.substring(0, 5), 18, y, 1);
    tft.drawString(weatherShortText(dailyForecasts[i].weatherCode).substring(0, 6), 70, y, 1);
    String range = formatCompactTemp(dailyForecasts[i].maxC) + "/" + formatCompactTemp(dailyForecasts[i].minC);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawRightString(range.c_str(), 176, y, 1);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    String rain = isnan(dailyForecasts[i].precipProb) ? "--" : String((int)roundf(dailyForecasts[i].precipProb)) + "%";
    tft.drawRightString(rain.c_str(), 222, y, 1);
  }
}

void drawNotesPageFull() {
  tft.fillScreen(COL_BG);
  drawTopBar("Notes");
  drawNavBar();

  drawCard(8, 40, 224, 126, true);
  drawCard(8, 174, 224, 98, false);

  pageDirty = false;
  lastDrawnPage = PAGE_NOTES;
  lastNotesText = "";
  lastChecklistText = "";
  lastInsightText = "";
}

void updateNotesDynamic() {
  String insightKey = insightTitle + "|" + insightBody + "|" + insightSource + "|" + insightStatus;
  String checklistKey = "";
  for (int i = 0; i < CHECKLIST_COUNT; i++) {
    checklistKey += String(checklistDone[i] ? 1 : 0) + checklistItems[i] + "|";
  }

  if (checklistKey != lastChecklistText || notesDirty) {
    tft.fillRect(18, 52, 204, 100, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("Checklist", 18, 52, 2);
    for (int i = 0; i < CHECKLIST_COUNT; i++) {
      int y = 76 + i * 18;
      uint16_t boxColor = checklistDone[i] ? COL_ACCENT : COL_STROKE;
      tft.drawRoundRect(18, y, 12, 12, 3, boxColor);
      if (checklistDone[i]) {
        tft.drawLine(21, y + 6, 24, y + 9, COL_ACCENT);
        tft.drawLine(24, y + 9, 29, y + 3, COL_ACCENT);
      }
      tft.setTextColor(checklistDone[i] ? COL_DIM : COL_TEXT, COL_PANEL);
      tft.drawString(checklistItems[i].substring(0, 28), 36, y - 1, 1);
    }
    lastChecklistText = checklistKey;
  }

  if (notesText != lastNotesText || insightKey != lastInsightText || notesDirty) {
    tft.fillRect(18, 186, 204, 72, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("Live", 18, 186, 1);
    tft.setTextColor(COL_ACCENT, COL_PANEL);
    tft.drawString(insightTitle.substring(0, 24), 18, 200, 2);
    drawWrappedTextLimited(18, 222, 198, insightBody, 1, COL_TEXT, COL_PANEL, 2);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString(insightSource.substring(0, 18), 18, 256, 1);
    tft.drawRightString(insightStatus.substring(0, 20).c_str(), 222, 256, 1);

    lastNotesText = notesText;
    lastInsightText = insightKey;
  }
  notesDirty = false;
}

void drawStatusPageFull() {
  tft.fillScreen(COL_BG);
  drawTopBar("Status");
  drawNavBar();

  drawCard(8, PAGE_ROW1_Y, 108, PAGE_WIDGET_H, true);
  drawCard(124, PAGE_ROW1_Y, 108, PAGE_WIDGET_H, true);
  drawCard(8, PAGE_ROW2_Y, 224, PAGE_WIDGET_H, true);
  drawCard(8, PAGE_ROW3_Y, 108, PAGE_WIDGET_H, true);
  drawCard(124, PAGE_ROW3_Y, 108, PAGE_WIDGET_H, true);

  pageDirty = false;
  lastDrawnPage = PAGE_STATUS;

  lastWifiText = "";
  lastSignalText = "";
  lastIpText = "";
  lastUptimeText = "";
  lastNetworkToggleText = "";
}

void updateStatusDynamic() {
  String w = wifiStatusText();
  if (w != lastWifiText) {
    tft.fillRect(18, PAGE_ROW1_Y + 24, 88, 30, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("WiFi", 18, PAGE_ROW1_Y + 8, 2);
    tft.setTextColor(statusColor(), COL_PANEL);
    tft.drawString(w, 18, PAGE_ROW1_Y + 32, 2);
    lastWifiText = w;
  }

  String s = signalText();
  if (s != lastSignalText) {
    tft.fillRect(134, PAGE_ROW1_Y + 30, 88, 24, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("Signal", 134, PAGE_ROW1_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(s, 134, PAGE_ROW1_Y + 30, 4);
    lastSignalText = s;
  }

  String ip = ipText();
  if (ip != lastIpText) {
    tft.fillRect(18, PAGE_ROW2_Y + 30, 200, 18, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("IP address", 18, PAGE_ROW2_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(ip, 18, PAGE_ROW2_Y + 30, 2);
    lastIpText = ip;
  }

  String up = uptimeText();
  String upCombined = up + "|" + lastSyncText();
  if (upCombined != lastUptimeText) {
    tft.fillRect(18, PAGE_ROW3_Y + 26, 88, 26, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("Uptime", 18, PAGE_ROW3_Y + 10, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(up, 18, PAGE_ROW3_Y + 26, 2);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString(lastSyncText(), 18, PAGE_ROW3_Y + 42, 1);
    lastUptimeText = upCombined;
  }

  String networkLabel = wifiEnabled ? "Enabled" : "Disabled";
  if (networkLabel != lastNetworkToggleText) {
    uint16_t btnBg = wifiEnabled ? COL_ACCENT : COL_PANEL_ALT;
    uint16_t btnFg = wifiEnabled ? TFT_BLACK : COL_TEXT;
    uint16_t btnStroke = wifiEnabled ? COL_ACCENT : COL_STROKE;

    tft.fillRect(132, PAGE_ROW3_Y + 8, 92, 42, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("Network", 134, PAGE_ROW3_Y + 10, 2);
    tft.fillRoundRect(134, PAGE_ROW3_Y + 30, 88, 22, 8, btnBg);
    tft.drawRoundRect(134, PAGE_ROW3_Y + 30, 88, 22, 8, btnStroke);
    tft.setTextColor(btnFg, btnBg);
    tft.drawCentreString(networkLabel.c_str(), 178, PAGE_ROW3_Y + 32, 2);
    lastNetworkToggleText = networkLabel;
  }
}

String contentModeLabel() {
  if (contentMode == "mix") return "Mix";
  if (contentMode == "tech") return "Tech";
  if (contentMode == "off") return "Off";
  return "Quote";
}

int currentCityPresetIndex() {
  for (int i = 0; i < CITY_PRESET_COUNT; i++) {
    if (fabsf(LAT - CITY_PRESETS[i].lat) < 0.02f && fabsf(LNG - CITY_PRESETS[i].lng) < 0.02f) return i;
  }
  return 0;
}

void saveCurrentLocation() {
  prefs.putString("locname", locationName);
  prefs.putFloat("lat", LAT);
  prefs.putFloat("lng", LNG);
  resetDataCaches();
}

void applyCityPreset(int direction) {
  int idx = currentCityPresetIndex();
  idx = (idx + direction + CITY_PRESET_COUNT) % CITY_PRESET_COUNT;
  locationName = CITY_PRESETS[idx].name;
  LAT = CITY_PRESETS[idx].lat;
  LNG = CITY_PRESETS[idx].lng;
  saveCurrentLocation();
}

void cycleContentMode() {
  if (contentMode == "mix") contentMode = "quote";
  else if (contentMode == "quote") contentMode = "tech";
  else if (contentMode == "tech") contentMode = "off";
  else contentMode = "mix";
  prefs.putString("contentMode", contentMode);
  lastInsightFetch = 0;
  lastInsightAttempt = 0;
  insightFailureCount = 0;
  lastInsightText = "";
  notesDirty = true;
  pageDirty = true;
  ensureInsight();
}

void adjustBrightness(int delta) {
  BL_FULL = constrain(BL_FULL + delta, 30, 255);
  prefs.putInt("brightness", BL_FULL);
  if (!sleepOff && !sleepDimmed) setBacklight(BL_FULL);
  lastSettingsText = "";
  pageDirty = true;
}

void drawSettingsPageFull() {
  tft.fillScreen(COL_BG);
  drawTopBar("Setup");
  drawNavBar();

  drawCard(8, PAGE_ROW1_Y, 224, PAGE_WIDGET_H, true);
  drawCard(8, PAGE_ROW2_Y, 224, PAGE_WIDGET_H, true);
  drawCard(8, PAGE_ROW3_Y, 108, PAGE_WIDGET_H, true);
  drawCard(124, PAGE_ROW3_Y, 108, PAGE_WIDGET_H, true);

  pageDirty = false;
  lastDrawnPage = PAGE_SETTINGS;
  lastSettingsText = "";
}

void updateSettingsDynamic() {
  String key = String(BL_FULL) + "|" + locationName + "|" + contentMode + "|" + wifiStatusText() + "|" + insightStatus;
  if (key == lastSettingsText) return;

  tft.fillRect(18, PAGE_ROW1_Y + 8, 204, 54, COL_PANEL);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString("Brightness", 18, PAGE_ROW1_Y + 8, 2);
  tft.setTextColor(COL_TEXT, COL_PANEL);
  tft.drawString("-     " + String((BL_FULL * 100) / 255) + "%     +", 24, PAGE_ROW1_Y + 34, 2);

  tft.fillRect(18, PAGE_ROW2_Y + 8, 204, 54, COL_PANEL);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString("City", 18, PAGE_ROW2_Y + 8, 2);
  tft.setTextColor(COL_TEXT, COL_PANEL);
  tft.drawString("< " + locationName + " >", 18, PAGE_ROW2_Y + 34, 2);

  tft.fillRect(18, PAGE_ROW3_Y + 8, 88, 54, COL_PANEL);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString("Live card", 18, PAGE_ROW3_Y + 8, 2);
  tft.setTextColor(COL_TEXT, COL_PANEL);
  tft.drawString(contentModeLabel(), 18, PAGE_ROW3_Y + 34, 2);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString(insightStatus.substring(0, 12), 18, PAGE_ROW3_Y + 52, 1);

  tft.fillRect(134, PAGE_ROW3_Y + 8, 88, 54, COL_PANEL);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString("WiFi", 134, PAGE_ROW3_Y + 8, 2);
  tft.setTextColor(statusColor(), COL_PANEL);
  tft.drawString(wifiStatusText(), 134, PAGE_ROW3_Y + 30, 1);
  tft.setTextColor(COL_ACCENT, COL_PANEL);
  tft.drawString("Setup AP", 134, PAGE_ROW3_Y + 46, 1);

  lastSettingsText = key;
}

void drawCurrentPageFull() {
  switch (currentPage) {
    case PAGE_HOME:    drawHomePageFull(); break;
    case PAGE_WEATHER: drawWeatherPageFull(); break;
    case PAGE_NOTES:   drawNotesPageFull(); break;
    case PAGE_STATUS:  drawStatusPageFull(); break;
    case PAGE_SETTINGS: drawSettingsPageFull(); break;
  }

  if (focusMenuOpen && currentPage == PAGE_HOME) drawFocusMenuOverlay(true);
  if (timerDoneDialogOpen) drawTimerDoneOverlay(true);
}

void updateCurrentPageDynamic() {
  if (timerDoneDialogOpen) {
    drawTimerDoneOverlay(false);
    return;
  }

  if (focusMenuOpen && currentPage == PAGE_HOME) {
    drawFocusMenuOverlay(false);
    return;
  }

  switch (currentPage) {
    case PAGE_HOME:    updateHomeDynamic(); break;
    case PAGE_WEATHER: updateWeatherDynamic(); break;
    case PAGE_NOTES:   updateNotesDynamic(); break;
    case PAGE_STATUS:  updateStatusDynamic(); break;
    case PAGE_SETTINGS: updateSettingsDynamic(); break;
  }
}

bool handleFocusMenuTouch(int x, int y) {
  if (!focusMenuOpen) return false;

  if (x < TIMER_MENU_X || x >= TIMER_MENU_X + TIMER_MENU_W || y < TIMER_MENU_Y || y >= TIMER_MENU_Y + TIMER_MENU_H) {
    focusMenuOpen = false;
    cacheTimerMenu = "";
    pageDirty = true;
    return true;
  }

  struct ButtonHit {
    int x;
    int y;
    int w;
    int h;
    int minutes;
  };

  const ButtonHit buttons[] = {
    {34, 124, 74, 28, timerPresetMin[0]},
    {132, 124, 74, 28, timerPresetMin[1]},
    {34, 158, 74, 28, timerPresetMin[2]},
    {132, 158, 74, 28, timerPresetMin[3]},
    {34, 192, 74, 28, timerPresetMin[4]},
    {132, 192, 74, 28, timerPresetMin[5]}
  };

  for (const ButtonHit& btn : buttons) {
    if (x >= btn.x && x < btn.x + btn.w && y >= btn.y && y < btn.y + btn.h) {
      startFocusTimer(btn.minutes);
      pageDirty = true;
      return true;
    }
  }

  if ((focusTimerRunning || focusTimerFinished) && x >= 44 && x < 196 && y >= 224 && y < 250) {
    if (focusTimerRunning || focusTimerFinished) {
      resetFocusTimer();
    } else {
      focusMenuOpen = false;
      cacheTimerMenu = "";
    }
    pageDirty = true;
    return true;
  }

  return true;
}

bool handleHomeTouch(int x, int y) {
  if (currentPage != PAGE_HOME) return false;

  if (focusMenuOpen) return handleFocusMenuTouch(x, y);

  if ((homeShowSlide == 0 && x >= 8 && x < 116 && y >= 166 && y < 242) ||
      (homeShowSlide == 2 && x >= 8 && x < 232 && y >= 62 && y < 132)) {
    if (focusTimerFinished) {
      resetFocusTimer();
    } else {
      focusMenuOpen = true;
      cacheHomeShow = "";
      cacheTimerMenu = "";
    }
    pageDirty = true;
    return true;
  }

  if (homeShowSlide == 2) {
    for (int i = 0; i < CHECKLIST_COUNT; i++) {
      int rowY = 150 + i * 22;
      if (x >= 8 && x < 232 && y >= rowY && y < rowY + 22) {
        checklistDone[i] = !checklistDone[i];
        String doneKey = String("checkDone") + String(i);
        prefs.putBool(doneKey.c_str(), checklistDone[i]);
        cacheHomeShow = "";
        notesDirty = true;
        pageDirty = true;
        return true;
      }
    }
  }

  if (y >= TOPBAR_H && y < SCREEN_H - NAV_H) {
    if (x < SCREEN_W / 3) {
      homeShowSlide = (homeShowSlide + HOME_SHOW_SLIDE_COUNT - 1) % HOME_SHOW_SLIDE_COUNT;
      lastHomeShowSlideMs = millis();
      cacheHomeShow = "";
      pageDirty = true;
      return true;
    }
    advanceHomeShowSlide();
    return true;
  }

  return false;
}

bool handleTimerDoneDialogTouch(int x, int y) {
  (void)x;
  (void)y;
  if (!timerDoneDialogOpen) return false;
  dismissTimerDoneDialog();
  return true;
}

bool handleStatusTouch(int x, int y) {
  if (currentPage != PAGE_STATUS) return false;

  if (x >= 124 && x < 232 && y >= 198 && y < 268) {
    setWifiEnabled(!wifiEnabled);
    return true;
  }

  return false;
}

bool handleNotesTouch(int x, int y) {
  if (currentPage != PAGE_NOTES) return false;

  for (int i = 0; i < CHECKLIST_COUNT; i++) {
    int rowY = 72 + i * 18;
    if (x >= 8 && x < 232 && y >= rowY && y < rowY + 18) {
      checklistDone[i] = !checklistDone[i];
      String doneKey = String("checkDone") + String(i);
      prefs.putBool(doneKey.c_str(), checklistDone[i]);
      lastChecklistText = "";
      notesDirty = true;
      pageDirty = true;
      return true;
    }
  }

  return false;
}

bool handleSettingsTouch(int x, int y) {
  if (currentPage != PAGE_SETTINGS) return false;

  if (y >= PAGE_ROW1_Y && y < PAGE_ROW1_Y + PAGE_WIDGET_H) {
    if (x < 88) adjustBrightness(-25);
    else if (x > 152) adjustBrightness(25);
    return true;
  }

  if (y >= PAGE_ROW2_Y && y < PAGE_ROW2_Y + PAGE_WIDGET_H) {
    applyCityPreset(x < SCREEN_W / 2 ? -1 : 1);
    return true;
  }

  if (x >= 8 && x < 116 && y >= PAGE_ROW3_Y && y < PAGE_ROW3_Y + PAGE_WIDGET_H) {
    cycleContentMode();
    return true;
  }

  if (x >= 124 && x < 232 && y >= PAGE_ROW3_Y && y < PAGE_ROW3_Y + PAGE_WIDGET_H) {
    wifiPortalRequested = true;
    lastSettingsText = "";
    pageDirty = true;
    return true;
  }

  return false;
}

// =========================================================
// NAVIGATION
// =========================================================
void handleNavTouch(int x, int y) {
  if (y < SCREEN_H - NAV_H) return;

  int btnW = SCREEN_W / NAV_COUNT;
  int idx = x / btnW;
  if (idx < 0 || idx >= NAV_COUNT) return;

  Page newPage = (Page)idx;
  if (newPage != currentPage) {
    currentPage = newPage;
    pageDirty = true;
  }
}

// =========================================================
// WEB SERVER
// =========================================================
void handleRoot() {
  String accent = prefs.getString("accent", "ice");
  String bg     = prefs.getString("bg", "graphite");
  String txt    = prefs.getString("text", "standard");
  String units  = prefs.getString("units", "metric");
  String region = prefs.getString("region", "europe");
  bool flashMode = prefs.getBool("flashMode", false);
  int brightness = prefs.getInt("brightness", BL_FULL);
  String liveMode = prefs.getString("contentMode", contentMode);
  if (liveMode != "mix" && liveMode != "quote" && liveMode != "tech" && liveMode != "off") liveMode = "mix";
  String homeSlotKeys[HOME_SLOT_COUNT];
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    homeSlotKeys[i] = prefs.getString((String("homeSlot") + String(i)).c_str(), homeWidgetKey(homeWidgetSlots[i]));
  }
  DailyReading reading = dailyReading();

  String page;
  page.reserve(30000);

  page += "<!doctype html><html><head>";
  page += "<meta charset='utf-8'>";
  page += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  page += "<title>Deskbuddy</title>";
  page += "<style>";
  page += ":root{color-scheme:dark;}";
  page += "body{margin:0;background:#0b0d10;color:#f5f5f7;font-family:-apple-system,BlinkMacSystemFont,'SF Pro Display','Segoe UI',system-ui,sans-serif;}";
  page += ".wrap{max-width:980px;margin:0 auto;padding:28px 16px 36px;}";
  page += ".hero{margin-bottom:16px;padding:22px;border:1px solid #2c2f36;border-radius:22px;background:linear-gradient(145deg,#181b20 0%,#101215 100%);box-shadow:0 18px 60px rgba(0,0,0,.32);}";
  page += ".hero h1{font-size:34px;margin:0 0 8px 0;letter-spacing:0;}";
  page += ".hero p{margin:0;color:#b8bec7;font-size:14px;line-height:1.5;}";
  page += ".ip{display:inline-block;margin-top:14px;padding:8px 12px;border-radius:999px;background:#1c1f24;border:1px solid #3a4049;color:#edf2f7;font-size:13px;}";
  page += ".summary{display:grid;grid-template-columns:repeat(4,1fr);gap:12px;margin-bottom:16px;}";
  page += ".summary-card{border:1px solid #2c2f36;border-radius:18px;background:#15181d;padding:14px;min-height:74px;}";
  page += ".summary-card span{display:block;color:#9aa3ad;font-size:12px;margin-bottom:8px;}";
  page += ".summary-card strong{display:block;color:#f5f5f7;font-size:16px;line-height:1.25;}";
  page += ".reading-card{grid-column:1 / -1;border:1px solid #334155;border-radius:16px;background:linear-gradient(145deg,#101820,#15181d);padding:14px;}";
  page += ".reading-card span{display:block;color:#8ea3ba;font-size:12px;font-weight:700;margin-bottom:8px;}";
  page += ".reading-card strong{display:block;color:#f5f5f7;font-size:18px;margin-bottom:8px;}";
  page += ".reading-card p{margin:0;color:#b8c4d4;font-size:13px;line-height:1.45;}";
  page += ".layout{display:grid;grid-template-columns:1.15fr .85fr;gap:16px;align-items:start;}";
  page += ".stack{display:grid;gap:16px;}";
  page += ".panel{background:#15181d;border:1px solid #2c2f36;border-radius:18px;padding:18px;margin:0;}";
  page += ".panel-toggle{width:100%;display:flex;align-items:center;justify-content:space-between;gap:12px;background:none;border:none;color:#edf2f7;padding:0;margin:0;cursor:pointer;text-align:left;}";
  page += ".panel-toggle:hover{color:#ffffff;}";
  page += ".panel-toggle h2{flex:1;}";
  page += ".panel-chevron{font-size:18px;color:#8ea3ba;transition:transform .18s ease;}";
  page += ".panel.collapsed .panel-chevron{transform:rotate(-90deg);}";
  page += ".panel-body{margin-top:12px;}";
  page += ".panel.collapsed .panel-body{display:none;}";
  page += ".panel h2{margin:0 0 6px 0;font-size:18px;}";
  page += ".panel p{margin:0 0 14px 0;color:#94a3b8;font-size:13px;line-height:1.45;}";
  page += ".grid{display:grid;grid-template-columns:1fr 1fr;gap:14px;}";
  page += ".grid-3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:14px;}";
  page += ".label{display:block;font-size:13px;margin:0 0 8px 0;color:#a0aec0;font-weight:600;}";
  page += "textarea,input,select{width:100%;border-radius:12px;border:1px solid #343a42;background:#0f1115;color:#f5f5f7;padding:12px;box-sizing:border-box;font:inherit;}";
  page += "textarea{min-height:170px;resize:vertical;}";
  page += "button{margin-top:18px;background:#f5f5f7;border:none;color:#0b0d10;padding:13px 18px;border-radius:999px;font-weight:800;cursor:pointer;font:inherit;}";
  page += ".muted{font-size:13px;color:#94a3b8;line-height:1.45;}";
  page += ".footer-note{margin-top:10px;font-size:12px;color:#7f92a8;}";
  page += ".settings-block{margin-top:18px;padding-top:16px;border-top:1px solid #2b3545;}";
  page += ".settings-block:first-of-type{margin-top:0;padding-top:0;border-top:none;}";
  page += ".settings-title{display:block;margin:0 0 6px 0;font-size:14px;font-weight:700;color:#edf2f7;letter-spacing:.02em;}";
  page += ".settings-desc{margin:0 0 12px 0;font-size:12px;color:#8ea3ba;line-height:1.45;}";
  page += ".color-stack{display:grid;gap:12px;}";
  page += ".color-row{display:grid;grid-template-columns:120px 1fr;gap:12px;align-items:center;}";
  page += ".color-meta{display:flex;align-items:center;justify-content:space-between;gap:10px;}";
  page += ".color-meta .label{margin:0;color:#dbe7f5;}";
  page += ".color-value{font-size:12px;color:#8ea3ba;white-space:nowrap;}";
  page += ".swatch-row{display:flex;flex-wrap:wrap;gap:8px;}";
  page += ".swatch{width:22px;height:22px;border-radius:999px;border:1px solid rgba(255,255,255,.18);cursor:pointer;position:relative;box-sizing:border-box;}";
  page += ".swatch input{display:none;}";
  page += ".swatch.active{box-shadow:0 0 0 2px #67e8f9, 0 0 0 5px rgba(103,232,249,.18);}";
  page += ".swatch.active::after{content:'';position:absolute;inset:5px;border-radius:999px;border:1px solid rgba(0,16,24,.45);}";
  page += ".timer-slot-grid{display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;margin-top:14px;}";
  page += ".timer-slot{border:1px solid #334155;border-radius:12px;background:#0b1220;padding:10px 10px 12px 10px;}";
  page += ".timer-slot-head{font-size:12px;color:#8ea3ba;margin-bottom:8px;font-weight:600;}";
  page += ".timer-slot-input{display:flex;align-items:center;gap:8px;}";
  page += ".timer-slot input{padding:10px 12px;text-align:center;font-weight:700;}";
  page += ".timer-unit{font-size:12px;color:#8ea3ba;white-space:nowrap;}";
  page += "@media(max-width:820px){.layout,.summary{grid-template-columns:1fr;}.grid,.grid-3,.timer-slot-grid{grid-template-columns:1fr;}.color-row{grid-template-columns:1fr;}}";
  page += "</style></head><body><div class='wrap'>";
  page += "<div class='hero'>";
  page += "<h1>Deskbuddy</h1>";
  page += "<p>Adjust notes, colors, system settings, and location from your browser.</p>";
  page += "<div class='ip'>ESP IP: ";
  page += WiFi.localIP().toString();
  page += "</div></div>";

  page += "<div class='summary'>";
  page += "<div class='summary-card'><span>Wi-Fi</span><strong>" + wifiStatusText() + "</strong></div>";
  page += "<div class='summary-card'><span>Weather</span><strong>" + tempText() + " / " + weatherConditionText(weatherCode) + "</strong></div>";
  page += "<div class='summary-card'><span>Psalm</span><strong>" + String(reading.ref) + "</strong></div>";
  page += "<div class='summary-card'><span>Sync</span><strong>" + lastSyncText() + "</strong></div>";
  page += "</div>";

  page += "<form method='POST' action='/save'>";
  page += "<div class='layout'><div class='stack'>";

  page += "<div class='panel' data-panel='notes'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>Notes</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>Short notes synced to the device.</p>";
  page += "<label class='label'>Notes</label>";
  page += "<textarea name='notes' maxlength='700'>";
  page += htmlEscape(notesText);
  page += "</textarea>";
  page += "<div class='settings-block'>";
  page += "<span class='settings-title'>Touch checklist</span>";
  page += "<div class='settings-desc'>Edit the labels here, then tap each row on the device to mark it done.</div>";
  page += "<div class='grid'>";
  for (int i = 0; i < CHECKLIST_COUNT; i++) {
    page += "<div><label class='label'>Item " + String(i + 1) + "</label>";
    page += "<input name='checkText" + String(i) + "' maxlength='40' value='" + htmlEscape(checklistItems[i]) + "'>";
    page += "<label style='display:flex;align-items:center;gap:8px;margin-top:8px;color:#a0aec0;font-size:13px;'>";
    page += "<input style='width:auto;' type='checkbox' name='checkDone" + String(i) + "'" + String(checklistDone[i] ? " checked" : "") + "> Done";
    page += "</label></div>";
  }
  page += "</div></div>";
  page += "<div class='muted'>Saved notes show up right away.</div>";
  page += "</div></div>";

  page += "<div class='panel' data-panel='theme'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>Theme and color</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>Colors and visual style for the display.</p>";
  page += "<div class='grid'>";

  page += "<div style='grid-column:1 / -1;' class='color-stack'>";

  page += "<div class='color-row'><div class='color-meta'><label class='label'>Accent</label><span class='color-value' id='accent-value'>";
  page += accent;
  page += "</span></div><div class='swatch-row'>";
  page += "<label class='swatch" + String(accent=="standard"?" active":"") + "' style='background:" + accentPreviewCss("standard") + ";'><input type='radio' name='accent' value='standard'" + String(accent=="standard"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="ice"?" active":"") + "' style='background:" + accentPreviewCss("ice") + ";'><input type='radio' name='accent' value='ice'" + String(accent=="ice"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="white"?" active":"") + "' style='background:" + accentPreviewCss("white") + ";'><input type='radio' name='accent' value='white'" + String(accent=="white"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="cyan"?" active":"") + "' style='background:" + accentPreviewCss("cyan") + ";'><input type='radio' name='accent' value='cyan'" + String(accent=="cyan"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="mint"?" active":"") + "' style='background:" + accentPreviewCss("mint") + ";'><input type='radio' name='accent' value='mint'" + String(accent=="mint"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="green"?" active":"") + "' style='background:" + accentPreviewCss("green") + ";'><input type='radio' name='accent' value='green'" + String(accent=="green"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="blue"?" active":"") + "' style='background:" + accentPreviewCss("blue") + ";'><input type='radio' name='accent' value='blue'" + String(accent=="blue"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="purple"?" active":"") + "' style='background:" + accentPreviewCss("purple") + ";'><input type='radio' name='accent' value='purple'" + String(accent=="purple"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="pink"?" active":"") + "' style='background:" + accentPreviewCss("pink") + ";'><input type='radio' name='accent' value='pink'" + String(accent=="pink"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="orange"?" active":"") + "' style='background:" + accentPreviewCss("orange") + ";'><input type='radio' name='accent' value='orange'" + String(accent=="orange"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="amber"?" active":"") + "' style='background:" + accentPreviewCss("amber") + ";'><input type='radio' name='accent' value='amber'" + String(accent=="amber"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="red"?" active":"") + "' style='background:" + accentPreviewCss("red") + ";'><input type='radio' name='accent' value='red'" + String(accent=="red"?" checked":"") + "></label>";
  page += "</div></div>";

  page += "<div class='color-row'><div class='color-meta'><label class='label'>Text</label><span class='color-value' id='text-value'>";
  page += txt;
  page += "</span></div><div class='swatch-row'>";
  page += "<label class='swatch" + String(txt=="standard"?" active":"") + "' style='background:" + accentPreviewCss("standard") + ";'><input type='radio' name='text' value='standard'" + String(txt=="standard"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="ice"?" active":"") + "' style='background:" + accentPreviewCss("ice") + ";'><input type='radio' name='text' value='ice'" + String(txt=="ice"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="white"?" active":"") + "' style='background:" + accentPreviewCss("white") + ";'><input type='radio' name='text' value='white'" + String(txt=="white"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="cyan"?" active":"") + "' style='background:" + accentPreviewCss("cyan") + ";'><input type='radio' name='text' value='cyan'" + String(txt=="cyan"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="mint"?" active":"") + "' style='background:" + accentPreviewCss("mint") + ";'><input type='radio' name='text' value='mint'" + String(txt=="mint"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="green"?" active":"") + "' style='background:" + accentPreviewCss("green") + ";'><input type='radio' name='text' value='green'" + String(txt=="green"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="blue"?" active":"") + "' style='background:" + accentPreviewCss("blue") + ";'><input type='radio' name='text' value='blue'" + String(txt=="blue"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="purple"?" active":"") + "' style='background:" + accentPreviewCss("purple") + ";'><input type='radio' name='text' value='purple'" + String(txt=="purple"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="pink"?" active":"") + "' style='background:" + accentPreviewCss("pink") + ";'><input type='radio' name='text' value='pink'" + String(txt=="pink"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="orange"?" active":"") + "' style='background:" + accentPreviewCss("orange") + ";'><input type='radio' name='text' value='orange'" + String(txt=="orange"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="amber"?" active":"") + "' style='background:" + accentPreviewCss("amber") + ";'><input type='radio' name='text' value='amber'" + String(txt=="amber"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="red"?" active":"") + "' style='background:" + accentPreviewCss("red") + ";'><input type='radio' name='text' value='red'" + String(txt=="red"?" checked":"") + "></label>";
  page += "</div></div>";

  page += "<div class='color-row'><div class='color-meta'><label class='label'>Theme</label><span class='color-value' id='bg-value'>";
  page += bg;
  page += "</span></div><div class='swatch-row'>";
  page += "<label class='swatch" + String(bg=="slate"?" active":"") + "' style='background:" + themePreviewCss("slate") + ";'><input type='radio' name='bg' value='slate'" + String(bg=="slate"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="deep"?" active":"") + "' style='background:" + themePreviewCss("deep") + ";'><input type='radio' name='bg' value='deep'" + String(bg=="deep"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="nordic"?" active":"") + "' style='background:" + themePreviewCss("nordic") + ";'><input type='radio' name='bg' value='nordic'" + String(bg=="nordic"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="forest"?" active":"") + "' style='background:" + themePreviewCss("forest") + ";'><input type='radio' name='bg' value='forest'" + String(bg=="forest"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="coffee"?" active":"") + "' style='background:" + themePreviewCss("coffee") + ";'><input type='radio' name='bg' value='coffee'" + String(bg=="coffee"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="soft"?" active":"") + "' style='background:" + themePreviewCss("soft") + ";'><input type='radio' name='bg' value='soft'" + String(bg=="soft"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="midnight"?" active":"") + "' style='background:" + themePreviewCss("midnight") + ";'><input type='radio' name='bg' value='midnight'" + String(bg=="midnight"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="graphite"?" active":"") + "' style='background:" + themePreviewCss("graphite") + ";'><input type='radio' name='bg' value='graphite'" + String(bg=="graphite"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="platinum"?" active":"") + "' style='background:" + themePreviewCss("platinum") + ";'><input type='radio' name='bg' value='platinum'" + String(bg=="platinum"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="cupertino"?" active":"") + "' style='background:" + themePreviewCss("cupertino") + ";'><input type='radio' name='bg' value='cupertino'" + String(bg=="cupertino"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="glass"?" active":"") + "' style='background:" + themePreviewCss("glass") + ";'><input type='radio' name='bg' value='glass'" + String(bg=="glass"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="garnet"?" active":"") + "' style='background:" + themePreviewCss("garnet") + ";'><input type='radio' name='bg' value='garnet'" + String(bg=="garnet"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="ochre"?" active":"") + "' style='background:" + themePreviewCss("ochre") + ";'><input type='radio' name='bg' value='ochre'" + String(bg=="ochre"?" checked":"") + "></label>";
  page += "</div></div>";

  page += "</div>";

  page += "</div></div></div>";

  page += "<div class='panel' data-panel='settings'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>Settings</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>Core behavior and timer setup.</p>";
  page += "<div class='settings-block'>";
  page += "<span class='settings-title'>General</span>";
  page += "<div class='grid'>";
  page += "<div class='reading-card'><span>Daily Psalm</span><strong>" + String(reading.ref) + "</strong><p>" + htmlEscape(String(reading.text)) + "</p><p style='margin-top:8px;color:#67e8f9;'>" + htmlEscape(String(reading.prompt)) + "</p></div>";
  page += "<div><label class='label'>Auto sleep interval</label><select name='sleepMin'>";
  page += "<option value='0'"  + String(sleepIntervalMin==0?" selected":"")  + ">Never</option>";
  page += "<option value='1'"  + String(sleepIntervalMin==1?" selected":"")  + ">1 minute</option>";
  page += "<option value='5'"  + String(sleepIntervalMin==5?" selected":"")  + ">5 minutes</option>";
  page += "<option value='10'" + String(sleepIntervalMin==10?" selected":"") + ">10 minutes</option>";
  page += "<option value='30'" + String(sleepIntervalMin==30?" selected":"") + ">30 minutes</option>";
  page += "<option value='60'" + String(sleepIntervalMin==60?" selected":"") + ">1 hour</option>";
  page += "</select><div class='muted' style='margin-top:8px;'>Sleep dims the screen first, then turns it fully off after 60 seconds.</div></div>";
  page += "<div><label class='label'>Brightness</label><input type='range' min='30' max='255' name='brightness' value='" + String(brightness) + "'></div>";
  page += "<div><label class='label'>Live card</label><select name='contentMode'>";
  page += "<option value='mix'" + String(liveMode=="mix"?" selected":"") + ">Quotes and technology</option>";
  page += "<option value='quote'" + String(liveMode=="quote"?" selected":"") + ">Quote of the moment</option>";
  page += "<option value='tech'" + String(liveMode=="tech"?" selected":"") + ">Technology headline</option>";
  page += "<option value='off'" + String(liveMode=="off"?" selected":"") + ">Off</option>";
  page += "</select></div>";
  page += "<div><label class='label'>Measurement system</label><select name='units'>";
  page += "<option value='metric'"   + String(units=="metric"?" selected":"")   + ">Celsius / mm</option>";
  page += "<option value='imperial'" + String(units=="imperial"?" selected":"") + ">Fahrenheit / inches</option>";
  page += "</select></div>";
  page += "<div><label class='label'>Date format</label><select name='region'>";
  page += "<option value='europe'" + String(region=="europe"?" selected":"") + ">European: dd.mm.yyyy</option>";
  page += "<option value='us'" + String(region=="us"?" selected":"") + ">US: mm/dd/yyyy</option>";
  page += "</select></div>";
  page += "</div>";
  page += "</div>";
  page += "<div class='settings-block'><span class='settings-title'>Timer</span><div class='settings-desc'>Choose the six quick timers shown in the popup menu.</div><div class='timer-slot-grid'>";
  for (int i = 0; i < 6; i++) {
    page += "<div class='timer-slot'><div class='timer-slot-head'>Slot " + String(i + 1) + "</div><div class='timer-slot-input'><input type='number' min='1' max='180' name='timer" + String(i) + "' value='" + String(timerPresetMin[i]) + "'><span class='timer-unit'>min</span></div></div>";
  }
  page += "</div>";
  page += "<div style='margin-top:14px;'><span class='settings-title'>Alert behavior</span><label style='display:flex;align-items:center;gap:10px;color:#edf2f7;'><input type='checkbox' name='flashMode' value='1'" + String(flashMode ? " checked" : "") + " style='width:auto;'>Flash screen when timer ends</label></div></div>";
  page += "<div class='settings-block'><span class='settings-title'>Location</span><div class='settings-desc'>Used for weather data and sun times.</div><div class='grid-3'>";
  page += "<div><label class='label'>Location name</label><input name='locname' value='" + htmlEscape(locationName) + "'></div>";
  page += "<div><label class='label'>Latitude</label><input name='lat' value='" + String(LAT, 6) + "'></div>";
  page += "<div><label class='label'>Longitude</label><input name='lng' value='" + String(LNG, 6) + "'></div>";
  page += "</div><div class='footer-note'>Example Berlin: latitude 52.5200, longitude 13.4050.</div></div>";
  page += "</div></div>";

  page += "<div class='panel' data-panel='widgets'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>Widget Customization</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>Choose which widgets appear in the four Home slots below the clock card.</p>";
  page += "<div class='grid'>";
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    page += "<div><label class='label'>";
    page += homeSlotLabel(i);
    page += "</label><select name='homeSlot";
    page += String(i);
    page += "'>";
    appendHomeWidgetOptions(page, homeSlotKeys[i]);
    page += "</select></div>";
  }
  page += "</div>";
  page += "</div></div>";

  page += "</div><div class='stack'>";

  page += "<button type='submit'>Save to Deskbuddy</button>";
  page += "</div></div></form>";
  page += "<script>";
  page += "var colorNames={accent:{standard:'Standard',ice:'Ice',white:'White',cyan:'Cyan',mint:'Mint',green:'Green',blue:'Blue',purple:'Purple',pink:'Pink',orange:'Orange',amber:'Amber',red:'Red'},text:{standard:'Standard',ice:'Ice',white:'White',cyan:'Cyan',mint:'Mint',green:'Green',blue:'Blue',purple:'Purple',pink:'Pink',orange:'Orange',amber:'Amber',red:'Red'},bg:{slate:'Slate',deep:'Deep black',nordic:'Nordic blue',forest:'Forest',coffee:'Coffee',soft:'Soft dark',midnight:'Midnight',graphite:'Graphite',platinum:'Platinum',cupertino:'Cupertino',glass:'Glass',garnet:'Garnet',ochre:'Ochre'}};";
  page += "var panelStorageKey='deskbuddy-panel-state-v1';";
  page += "document.querySelectorAll('.swatch input').forEach(function(input){";
  page += "input.addEventListener('change',function(){";
  page += "document.querySelectorAll('.swatch input[name=\"'+input.name+'\"]').forEach(function(peer){";
  page += "peer.closest('.swatch').classList.toggle('active', peer.checked);";
  page += "});";
  page += "var valueEl=document.getElementById(input.name+'-value');";
  page += "if(valueEl&&colorNames[input.name]&&colorNames[input.name][input.value]){valueEl.textContent=colorNames[input.name][input.value];}";
  page += "});";
  page += "});";
  page += "function readPanelState(){try{return JSON.parse(localStorage.getItem(panelStorageKey)||'{}');}catch(e){return {};}}";
  page += "function writePanelState(state){localStorage.setItem(panelStorageKey,JSON.stringify(state));}";
  page += "function applyPanelState(panel,collapsed){panel.classList.toggle('collapsed',collapsed);var btn=panel.querySelector('.panel-toggle');if(btn){btn.setAttribute('aria-expanded',collapsed?'false':'true');}}";
  page += "var savedPanelState=readPanelState();";
  page += "document.querySelectorAll('.panel[data-panel]').forEach(function(panel){";
  page += "var panelId=panel.getAttribute('data-panel');";
  page += "if(Object.prototype.hasOwnProperty.call(savedPanelState,panelId)){applyPanelState(panel,!!savedPanelState[panelId]);}";
  page += "});";
  page += "document.querySelectorAll('.panel-toggle').forEach(function(btn){";
  page += "btn.addEventListener('click',function(){";
  page += "var panel=btn.closest('.panel');";
  page += "var collapsed=!panel.classList.contains('collapsed');";
  page += "applyPanelState(panel,collapsed);";
  page += "var state=readPanelState();";
  page += "var panelId=panel.getAttribute('data-panel');";
  page += "if(panelId){state[panelId]=collapsed;writePanelState(state);}";
  page += "});";
  page += "});";
  page += "</script>";
  page += "</div></body></html>";

  server.send(200, "text/html; charset=utf-8", page);
}

void handleSave() {
  String newNotes  = server.hasArg("notes") ? server.arg("notes") : notesText;
  String newAccent = server.hasArg("accent") ? server.arg("accent") : "ice";
  String newBg     = server.hasArg("bg") ? server.arg("bg") : "graphite";
  String newText   = server.hasArg("text") ? server.arg("text") : "standard";
  String newUnits  = server.hasArg("units") ? server.arg("units") : "metric";
  String newRegion = server.hasArg("region") ? server.arg("region") : "europe";
  String newLoc    = server.hasArg("locname") ? server.arg("locname") : locationName;
  String newNickname = server.hasArg("nickname") ? server.arg("nickname") : buddyNickname;
  String newContentMode = server.hasArg("contentMode") ? server.arg("contentMode") : contentMode;
  HomeWidgetType newHomeSlots[HOME_SLOT_COUNT];
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    String key = String("homeSlot") + String(i);
    String currentKey = homeWidgetKey(homeWidgetSlots[i]);
    newHomeSlots[i] = homeWidgetFromKey(server.hasArg(key) ? server.arg(key) : currentKey);
  }

  float newLat = server.hasArg("lat") ? server.arg("lat").toFloat() : LAT;
  float newLng = server.hasArg("lng") ? server.arg("lng").toFloat() : LNG;

  newNotes.trim();
  newLoc.trim();
  newNickname.trim();
  String newChecklistItems[CHECKLIST_COUNT];
  bool newChecklistDone[CHECKLIST_COUNT];
  for (int i = 0; i < CHECKLIST_COUNT; i++) {
    String textKey = String("checkText") + String(i);
    String doneKey = String("checkDone") + String(i);
    newChecklistItems[i] = server.hasArg(textKey) ? server.arg(textKey) : checklistItems[i];
    newChecklistItems[i].trim();
    if (newChecklistItems[i].length() == 0) newChecklistItems[i] = "Task " + String(i + 1);
    if (newChecklistItems[i].length() > 40) newChecklistItems[i] = newChecklistItems[i].substring(0, 40);
    newChecklistDone[i] = server.hasArg(doneKey);
  }

  if (newNotes.length() == 0) newNotes = "No notes yet.";
  if (newNotes.length() > 700) newNotes = newNotes.substring(0, 700);
  if (newLoc.length() == 0) newLoc = "Unknown";
  if (newNickname.length() > 24) newNickname = newNickname.substring(0, 24);
  if (newUnits != "metric" && newUnits != "imperial") newUnits = "metric";
  if (newRegion != "europe" && newRegion != "us") newRegion = "europe";
  if (newContentMode != "mix" && newContentMode != "quote" && newContentMode != "tech" && newContentMode != "off") newContentMode = "mix";

  int newSleepMin = server.hasArg("sleepMin") ? server.arg("sleepMin").toInt() : sleepIntervalMin;
  sleepIntervalMin = constrain(newSleepMin, 0, 120);
  BL_FULL = constrain(server.hasArg("brightness") ? server.arg("brightness").toInt() : BL_FULL, 30, 255);
  bool newFlashMode = server.hasArg("flashMode");

  bool locationChanged =
    (fabsf(newLat - LAT) > 0.0001f) ||
    (fabsf(newLng - LNG) > 0.0001f) ||
    (newLoc != locationName);

  notesText = newNotes;
  for (int i = 0; i < CHECKLIST_COUNT; i++) {
    checklistItems[i] = newChecklistItems[i];
    checklistDone[i] = newChecklistDone[i];
  }
  buddyNickname = newNickname;
  locationName = newLoc;
  LAT = newLat;
  LNG = newLng;
  unitKey = newUnits;
  regionFormatKey = newRegion;
  contentMode = newContentMode;
  flashModeEnabled = newFlashMode;
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    homeWidgetSlots[i] = newHomeSlots[i];
  }

  for (int i = 0; i < 6; i++) {
    String key = String("timer") + String(i);
    int currentValue = timerPresetMin[i];
    int nextValue = server.hasArg(key) ? server.arg(key).toInt() : currentValue;
    timerPresetMin[i] = sanitizeTimerMinutes(nextValue);
  }

  prefs.putString("notes", notesText);
  for (int i = 0; i < CHECKLIST_COUNT; i++) {
    String textKey = String("checkText") + String(i);
    String doneKey = String("checkDone") + String(i);
    prefs.putString(textKey.c_str(), checklistItems[i]);
    prefs.putBool(doneKey.c_str(), checklistDone[i]);
  }
  prefs.putString("accent", newAccent);
  prefs.putString("bg", newBg);
  prefs.putString("text", newText);
  prefs.putString("units", unitKey);
  prefs.putString("region", regionFormatKey);
  prefs.putString("nickname", buddyNickname);
  prefs.putString("contentMode", contentMode);
  prefs.putString("locname", locationName);
  prefs.putFloat("lat", LAT);
  prefs.putFloat("lng", LNG);
  prefs.putInt("sleepMin", sleepIntervalMin);
  prefs.putInt("brightness", BL_FULL);
  prefs.putBool("flashMode", flashModeEnabled);
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    String key = String("homeSlot") + String(i);
    prefs.putString(key.c_str(), homeWidgetKey(homeWidgetSlots[i]));
  }
  for (int i = 0; i < 6; i++) {
    String key = String("timer") + String(i);
    prefs.putInt(key.c_str(), timerPresetMin[i]);
  }

  applyThemeByKey(newAccent, newBg);
  applyTextColorByKey(newText);
  if (!sleepDimmed && !sleepOff) setBacklight(BL_FULL);

  notesDirty = true;
  pageDirty = true;
  dataDirty = true;

  cacheClock = "";
  cacheHomeEmpty1 = "";
  cacheHomeEmpty2 = "";
  cacheFocusTimer = "";
  cacheTimerMenu = "";
  cacheTimerDone = "";
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    cacheHomeSlots[i] = "";
  }

  lastTempText = "";
  lastRainText = "";
  lastKpText = "";
  lastKpLevelText = "";
  lastWindText = "";
  lastWindDirText = "";
  lastNextSunLabel = "";
  lastNextSunTime = "";
  lastUptimeText = "";

  if (locationChanged) resetDataCaches();
  lastInsightFetch = 0;
  lastInsightAttempt = 0;
  insightFailureCount = 0;
  lastInsightText = "";
  ensureInsight();

  server.sendHeader("Location", "/");
  server.send(303);
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}

// =========================================================
// SETUP / LOOP
// =========================================================
void waitForNtpTime() {
  time_t now = time(nullptr);
  unsigned long t0 = millis();
  while (now < 1700000000 && millis() - t0 < 10000) {
    delay(200);
    now = time(nullptr);
  }
}

void beginWiFiConnect() {
  if (!wifiEnabled) {
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    wifiConnectInProgress = false;
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  if (hasStaticWifiCredentials()) {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  } else {
    WiFi.begin();
  }
  wifiConnectInProgress = true;
  wifiConnectStartedMs = millis();
}

void startWifiPortal() {
  wifiConnectInProgress = false;
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  wm.setConnectTimeout(20);
  wm.setConfigPortalBlocking(true);
  tft.fillScreen(COL_BG);
  drawTopBar("WiFi setup");
  tft.setTextColor(COL_TEXT, COL_BG);
  drawWrappedTextLimited(18, 58, 204, "Connect your phone to Deskbuddy Setup, choose Wi-Fi, then return here.", 2, COL_TEXT, COL_BG, 8);
  wm.startConfigPortal("Deskbuddy Setup");
  beginWiFiConnect();
  pageDirty = true;
  lastSettingsText = "";
}

void connectWiFi(bool waitForConnection = true) {
  beginWiFiConnect();
  if (!waitForConnection) return;

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(200);
  }
  wifiConnectInProgress = false;
}

void updateWiFiConnectionState() {
  if (!wifiEnabled || !wifiConnectInProgress) return;

  wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    wifiConnectInProgress = false;
    ensureSunTimesForToday();
    ensureWeather();
    ensureKpIndex();
    ensureInsight();
    dataDirty = true;
    pageDirty = true;
    return;
  }

  if (millis() - wifiConnectStartedMs >= WIFI_CONNECT_TIMEOUT_MS) {
    wifiConnectInProgress = false;
    pageDirty = true;
  }
}

void setWifiEnabled(bool enabled) {
  wifiEnabled = enabled;
  prefs.putBool("wifiEnabled", wifiEnabled);

  if (!wifiEnabled) {
    wifiConnectInProgress = false;
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
  } else {
    beginWiFiConnect();
  }

  dataDirty = true;
  pageDirty = true;
}

void setup() {
  Serial.begin(115200);

  pinMode(BACKLIGHT_PIN, OUTPUT);
  analogWrite(BACKLIGHT_PIN, BL_FULL);
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);

  loadStoredSettings();
  setBacklight(BL_FULL);
  lastInteractionMs = millis();

  delay(200);

  tft.init();
  tft.setRotation(ROT);
  tft.invertDisplay(INV);
  tft.setSwapBytes(true);

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Booting Deskbuddy...", 10, 10, 2);

  touchSPI.begin(T_SCK, T_MISO, T_MOSI);
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);
  ts.begin(touchSPI);
  ts.setRotation(ROT);

  tft.drawString("Connecting WiFi...", 10, 34, 2);
  connectWiFi(true);

  tft.drawString("Syncing time...", 10, 58, 2);
  configTzTime("CET-1CEST,M3.5.0/2,M10.5.0/3",
               "pool.ntp.org", "time.google.com", "time.cloudflare.com");
  waitForNtpTime();

  ensureSunTimesForToday();
  ensureWeather();
  ensureKpIndex();
  ensureInsight();

  setupWebServer();

  pageDirty = true;
  dataDirty = true;
  notesDirty = true;

  drawCurrentPageFull();
  updateCurrentPageDynamic();

  lastClockTick = millis();
  lastDataTick = millis();

  Serial.print("Deskbuddy web: http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  server.handleClient();
  updateWiFiConnectionState();
  updateFocusTimerState();
  updateTimerDoneDialogState();
  handleAutoSleep();

  if (wifiPortalRequested) {
    wifiPortalRequested = false;
    startWifiPortal();
  }

  int tx = 0, ty = 0;
  if (touchNewPress(tx, ty)) {
    lastInteractionMs = millis();

    if (sleepOff) {
      if (manualDimMode) {
        sleepOff = false;
        sleepDimmed = true;
        setBacklight(BL_DIM);
        pageDirty = true;
      } else {
        wakeDisplay();
      }
      return;
    }

    if (handleTimerDoneDialogTouch(tx, ty)) {
      return;
    }

    if (tx >= SCREEN_W - 36 && ty <= TOPBAR_H) {
      toggleSleepMode();
      pageDirty = true;
    } else {
      if (sleepDimmed) {
        if (!manualDimMode) {
          wakeDisplay();
        } else {
          if (!handleHomeTouch(tx, ty) && !handleNotesTouch(tx, ty) && !handleStatusTouch(tx, ty) && !handleSettingsTouch(tx, ty)) {
            handleNavTouch(tx, ty);
          }
        }
      } else {
        if (!handleHomeTouch(tx, ty) && !handleNotesTouch(tx, ty) && !handleStatusTouch(tx, ty) && !handleSettingsTouch(tx, ty)) {
          handleNavTouch(tx, ty);
        }
      }
    }
  }

  if (millis() - lastDataTick >= DATA_TICK_MS) {
    lastDataTick = millis();
    ensureSunTimesForToday();
    ensureWeather();
    ensureKpIndex();
    ensureInsight();
  }

  if (pageDirty || lastDrawnPage != currentPage) {
    drawCurrentPageFull();
    updateCurrentPageDynamic();
    pageDirty = false;
    dataDirty = false;
  }

  if (currentPage == PAGE_HOME && !focusMenuOpen && !timerDoneDialogOpen &&
      millis() - lastHomeShowSlideMs >= HOME_SHOW_SLIDE_MS) {
    advanceHomeShowSlide();
  }

  if (millis() - lastClockTick >= CLOCK_TICK_MS) {
    lastClockTick = millis();
    updateCurrentPageDynamic();
    dataDirty = false;
  }

  delay(10);
}
