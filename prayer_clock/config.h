#pragma once
// All user settings. Wi-Fi is set from a phone (PrayerClock-Setup portal); keys and tokens live in secrets.h.

const char* const FW_VERSION = "0.3";
const char* const MODULE_MARKING = "N16R8";  // shown on the Status screen; set to what is printed on your module

// ---- Noor Al-Yaqeen website (API contract: NOOR_DEVICE_INTEGRATION.md) ----
const char* const NOOR_BASE_URL = "https://nooralyaqen.vercel.app/api/device";

// ---- locations (Telegram /city beirut|saida switches; the choice survives reboots) ----
struct City { const char* key; const char* nameAr; float lat, lon; };
const City CITIES[] = {
  {"beirut", "بيروت · سن الفيل", 33.875f, 35.53f},
  {"saida", "صيدا", 33.56f, 35.37f},
};
const int N_CITIES = sizeof(CITIES) / sizeof(CITIES[0]);
const int DEFAULT_CITY = 0;

// ---- prayer times: the website first, Aladhan /v1/timings only if the site fails ----
const int METHOD = 5;  // Aladhan fallback: Egyptian General Authority of Survey
const char* const TIMEZONE = "Asia/Beirut";
const int TUNE_FAJR = 0, TUNE_SUNRISE = 0, TUNE_DHUHR = 0, TUNE_ASR = 0, TUNE_MAGHRIB = 0, TUNE_ISHA = 0;  // minutes
// Aladhan fallback only (the website applies its own offset): days added to Aladhan's Hijri date. Its default
// calendar ignores the API's `adjustment`, so the clock shows the Hijri date Aladhan gives for today + HIJRI_ADJUST.
const int HIJRI_ADJUST = -1;
const char* const TZ_POSIX = "EET-2EEST,M3.5.0/0,M10.5.0/0";  // Asia/Beirut, for the ESP's own clock
const bool CLOCK_12H = true;  // the big clock; prayer times are always shown as 24-hour HH:MM

// ---- news (RSS; only the first ITEMS_PER_FEED items of each feed are downloaded) ----
struct Feed { const char* name; const char* url; };
const Feed FEEDS[] = {
  {"LBCI", "https://www.lbcgroup.tv/Rss/latest-news/ar"},
  {"الجزيرة", "https://www.aljazeera.net/aljazeerarss/a7c186be-1baa-4bd4-9d80-a84db769f779/73d0e1b4-532f-45ef-b135-bfdff8b8cab9"},
  {"BBC عربي", "https://feeds.bbci.co.uk/arabic/rss.xml"},
  {"فرانس 24", "https://www.france24.com/ar/rss"},
};
const int N_FEEDS = sizeof(FEEDS) / sizeof(FEEDS[0]);
const int ITEMS_PER_FEED = 3;
const uint32_t NEWS_EVERY_MS = 30UL * 60 * 1000;

// ---- schedules ----
const int PRAYER_FETCH_AFTER_MIN = 5;           // daily /prayer fetch after 00:05
const int NOOR_FETCH_AT_MIN = 8 * 60 + 1;       // /today at 08:01 ...
const uint32_t NOOR_EVERY_MS = 3UL * 3600 * 1000;  // ... and every 3 h
const uint32_t HEARTBEAT_MS = 60000;
const uint32_t TASBEEH_SYNC_MS = 60000;         // or every TASBEEH_SYNC_PRESSES presses
const int TASBEEH_SYNC_PRESSES = 20;
const int TASBEEH_CYCLE = 33;

// ---- behaviour ----
const long ALERT_SECONDS = 10 * 60;            // full-screen alert after each prayer starts
const int REMIND_BEFORE_MIN = 10;              // Telegram reminder before each prayer
const int DIGEST_AFTER_FAJR_MIN = 20;          // Telegram morning digest
const int ADHKAR_AFTER_MIN = 10;               // adhkar screen at Fajr+10 (morning) and Asr+10 (evening)
const int ADHKAR_FOR_MIN = 20;                 // ... for up to 20 minutes
const uint32_t SCREEN_TIMEOUT_MS = 30000;      // back to Home (not from Tasbeeh / Adhkar)
const uint32_t NEWS_LINE_MS = 8000;            // Home news line rotation
const int BRIGHT_NORMAL = 217, BRIGHT_ALERT = 255, BRIGHT_DIM = 64, BRIGHT_NIGHT = 38;  // 85%, 100%, 25%, 15%
const int DIM_AFTER_ISHA_MIN = 60, DIM_BEFORE_FAJR_MIN = 30;
const int BUTTON_PIN = 0;                      // BOOT
const char* const HOSTNAME = "prayer-clock";   // OTA: prayer-clock.local
const char* const PORTAL_SSID = "PrayerClock-Setup";
