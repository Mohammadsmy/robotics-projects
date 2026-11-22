// Prayer clock: ESP32-S3 + 3.5" 480x320 SPI screen (ILI9488 / ST7796S), Arabic right-to-left UI (DESIGN.md).
// LVGL 9 draws through LovyanGFX. Prayer times, daily post, adhkar and tasbeeh sync from the Noor Al-Yaqeen
// website (Aladhan as fallback), RSS news, Telegram bot, Wi-Fi portal and OTA. UI on core 1, network on core 0.
// Build and upload: see README.md.
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <UniversalTelegramBot.h>
#include <esp_task_wdt.h>
#include <algorithm>
#include <assert.h>
#include <lvgl.h>
#include "LGFX_config.h"
#include "config.h"
#include "secrets.h"
#include "certs.h"
#include "state.h"
#include "logic.h"
#include "rss.h"
#include "noor.h"
#include "net.h"
#include "ui.h"

String cps(std::initializer_list<uint32_t> l) {
  String s;
  for (uint32_t c : l) appendUtf8(s, c);
  return s;
}

void selfTest() {
  int16_t m[6] = {300, 380, 700, 900, 1000, 1080}, tmr[6] = {302, 381, 700, 900, 999, 1079};
  Span s = currentSpan(m, tmr, 0);  // before Fajr: from yesterday's Isha
  assert(s.next == 0 && s.prev == 5 && s.start == 1080 * 60L - 86400);
  s = currentSpan(m, tmr, 400 * 60);  // after sunrise: next is Dhuhr, counted from Fajr
  assert(s.next == 2 && s.prev == 0 && s.start == 300 * 60L);
  s = currentSpan(m, tmr, 700 * 60);  // exactly Dhuhr: it just started
  assert(s.next == 3 && s.prev == 2);
  s = currentSpan(m, tmr, 1100 * 60);  // after Isha: tomorrow's Fajr
  assert(s.next == 0 && s.prev == 5 && s.end == 302 * 60L + 86400);
  assert(currentSpan(m, nullptr, 1100 * 60).end == 300 * 60L + 86400);

  assert(parseHM("04:58") == 298 && parseHM("18:20 (EET)") == 1100 && parseHM("4:58") == -1 && parseHM("") == -1);
  assert(parseYmd("2026-11-19") == 20261119 && parseYmd("2026-13-01") == 0 && parseYmd("x") == 0);
  assert(parseRfc822("Thu, 19 Nov 2026 06:17:06 GMT") == 1795069026);
  assert(parseRfc822("Thu, 19 Nov 2026 06:26:58 +0300") == 1795058818);
  assert(parseRfc822("29 Feb 2024 23:59 GMT") == 1709251140 && parseRfc822("garbage") == 0);
  assert(parseIso("2026-11-19T06:17:06Z") == 1795069026 && parseIso("2026-11-19T06:17:06.000Z") == 1795069026);
  assert(parseIso("2026-11-19T06:26:58+03:00") == 1795058818 && parseIso("") == 0);

  RssParser p(2);
  const char* rss =
      "<rss><channel><title>Chan</title><item><title><![CDATA[ خبر   أول ]]></title>"
      "<pubDate>Thu, 19 Nov 2026 06:17:06 GMT</pubDate></item><item>\n<title>Second &amp; &#1576;</title>"
      "<media:content url='x'/><pubDate>Thu, 19 Nov 2026 06:26:58 +0300</pubDate></item><item><title>3</title></item>";
  for (const char* c = rss; *c && !p.done(); c++) p.feed(*c);
  assert(p.n == 2 && p.items[0].title == "خبر أول" && p.items[0].when == 1795069026);
  assert(p.items[1].title == "Second & ب" && p.items[1].when == 1795058818);

  // shaping: سلام -> seen initial, lam-alef final, meem isolated; marks never break a join
  assert(shapeArabic("سلام") == cps({0xFEB3, 0xFEFC, 0xFEE1}));
  assert(shapeArabic("بَّت") == cps({0xFE91, 0x064E, 0x0651, 0xFE96}));
  assert(shapeArabic("ء 12") == cps({0xFE80, ' ', '1', '2'}));
  assert(stripMarks("الصينيّ") == "الصيني");

  Button b;  // single, double, long press
  assert(b.update(true, 0, false) == BTN_NONE && b.update(false, 100, false) == BTN_SHORT);
  assert(b.update(true, 1000, true) == BTN_NONE && b.update(false, 1100, true) == BTN_NONE);
  assert(b.update(false, 1300, true) == BTN_NONE && b.update(false, 1501, true) == BTN_SHORT);
  b.update(true, 2000, true); b.update(false, 2100, true); b.update(true, 2200, true);
  assert(b.update(false, 2300, true) == BTN_DOUBLE);
  b.update(true, 3000, true);
  assert(b.update(true, 5000, true) == BTN_LONG && b.update(false, 5100, true) == BTN_NONE);

  Tasbeeh t = {};
  for (int i = 0; i < TASBEEH_CYCLE; i++) tasbeehPress(t);
  assert(t.phrase == 1 && t.count == 0 && t.pending[0] == TASBEEH_CYCLE);
  tasbeehPress(t);
  assert(tasbeehMakeBatch(t) && t.outPhrase == 0 && t.outDelta == TASBEEH_CYCLE && t.outSeq == 1 && !t.pending[0]);
  assert(tasbeehMakeBatch(t) && t.outSeq == 1);  // unsent batch is resent with the same seq
  assert(tasbeehToday(t, 20261119) == TASBEEH_CYCLE + 1);

  char buf[6];
  copyUtf8(buf, sizeof buf, "abخبر");  // 2 + 2 + 2 bytes: must stop before the cut letter
  assert(strcmp(buf, "abخ") == 0);
  assert(countAr(1, "دقيقة", "دقيقتين", "دقائق") == "دقيقة" && countAr(5, "دقيقة", "دقيقتين", "دقائق") == "5 دقائق");
  assert(countdownAr(2 * 3600 + 26 * 60) == "2:26" && countdownAr(45 * 60) == "45 د" && fmt24(1077) == "17:57");
  assert(fmtDuration(3725) == "1:02:05" && fmtDuration(65) == "1:05");
  assert(fabsf(sunX(0) - 400) < 0.01f && fabsf(sunX(1) - 56) < 0.01f && fabsf(sunY(0.5f) - 8) < 0.01f);
  Serial.println("self-test ok");
}

void setup() {
  Serial.begin(115200);
  delay(300);
  selfTest();
  lock = xSemaphoreCreateMutex();
  loadPersisted();
  esp_task_wdt_init(30, true);  // a task stuck for 30 s reboots the board
  xTaskCreatePinnedToCore(uiTask, "ui", 16384, nullptr, 2, nullptr, 1);
  xTaskCreatePinnedToCore(netTask, "net", 20480, nullptr, 1, nullptr, 0);
}

void loop() { vTaskDelete(nullptr); }  // all work happens in the two tasks
