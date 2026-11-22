# Prayer clock (ESP32-S3 + 3.5" SPI screen)

Arabic RTL desk display (layout: DESIGN.md / the design canvas, artboards 1-7):
Home with the sun path, نور اليوم, News, Tasbeeh, Status, the prayer alert and the adhkar screen.
Data: the Noor Al-Yaqeen website API (prayer times, daily post, adhkar, tasbeeh sync, heartbeat), Aladhan as the
prayer-time fallback, RSS news, Telegram bot. Wi-Fi setup portal, OTA, watchdog; UI on core 1, network on core 0.

Settings: `config.h`. Secrets: copy `secrets.h.example` to `secrets.h` (git-ignored).

## Libraries
LovyanGFX 1.2.34, lvgl 9.4.0, ArduinoJson 7.4, UniversalTelegramBot 1.3.0, WiFiManager 2.0.17; ESP32 core 2.0.7.

## Build and upload (arduino-cli)
LVGL must find this folder's `lv_conf.h`, including from its assembly files, hence the three `extra_flags`.
Use the board options that match the module marking (below is N16R8: 16 MB flash, OPI PSRAM).

```bash
arduino-cli compile --fqbn "esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB" --build-property "compiler.c.extra_flags=-I{build.source.path}" --build-property "compiler.cpp.extra_flags=-I{build.source.path}" --build-property "compiler.S.extra_flags=-I{build.source.path}" --upload -p COM8 .
```

Arduino IDE instead: copy `lv_conf.h` next to the `lvgl` folder in your Arduino `libraries` folder.

## First boot
Join the `PrayerClock-Setup` hotspot (password `PORTAL_PASSWORD`) from a phone and pick your Wi-Fi.
Later uploads can go over Wi-Fi to `prayer-clock.local` (password `OTA_PASSWORD`).
Serial (115200) prints `self-test ok` when the pure logic checks pass.

## Buttons (BOOT)
Short: Home → نور اليوم → News → Tasbeeh → Status → Home (News and a long نور اليوم page first).
On Tasbeeh / Adhkar: short = +1 (counted 400 ms after release), double = leave, long 2 s on Tasbeeh = reset the cycle.
Long press elsewhere: night mode (15% backlight).

## Notes
- TLS to the website trusts GTS Root R1 (what `*.vercel.app` uses) and ISRG Root X1 (`certs.h`).
- Fonts (`font_*.c`) come from lv_font_conv 1.5.2: Cairo (with its 36 missing isolated Arabic forms mapped to the
  base glyphs) for the UI, Amiri Bold for Quran / dhikr text. Text is shaped by `shapeArabic()` in `logic.h`,
  because LVGL's shaper breaks words with more than one mark (shadda + vowel, Quranic marks).

## Font licences
The `font_*.c` files are bitmap subsets of [Cairo](https://github.com/Gue3bara/Cairo) and
[Amiri](https://github.com/aliftype/amiri), both under the SIL Open Font License 1.1. Copyright belongs to their authors.
