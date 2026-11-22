#pragma once
// Network task (core 0): Wi-Fi portal, NTP, OTA, website sync, Aladhan fallback, RSS news, Telegram bot.
// Nothing here touches LVGL; results go into S under the Guard.

// ---------- retries ----------
struct Backoff {
  uint32_t next = 0, wait = 15000;
  bool due() const { return (int32_t)(millis() - next) >= 0; }
  void ok(uint32_t again) { wait = 15000; next = millis() + again; }
  void fail() { next = millis() + wait; wait = min<uint32_t>(wait * 2, 30UL * 60 * 1000); }
};
Backoff aladhanTry, newsTry, siteDays, siteNoor, siteAdhkar, siteTasbeeh, siteBeat;
uint32_t sitePrayerYmd = 0;  // the day the website's 7-day times were last fetched

String hijriFor(const State& s, time_t now) {
  const Day* d = findDay(s, ymdPlus(now, 0));
  if (d && d->fromSite) return d->hijri;  // the website applies its own offset
  const Day* a = findDay(s, ymdPlus(now, HIJRI_ADJUST));
  return a ? String(a->hijri) : String("");
}

// ---------- public HTTP (news, Aladhan) ----------
bool httpBegin(HTTPClient& http, WiFiClientSecure& c, const char* url) {
  c.setInsecure();  // shortcut: public data only (prayer fallback, news), no cert check; website and Telegram verify theirs
  http.setConnectTimeout(10000);
  http.setTimeout(10000);
  http.useHTTP10(true);  // no chunked encoding, so the stream is the plain body
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent("Mozilla/5.0 (ESP32 prayer clock)");
  if (!http.begin(c, url)) return false;
  int code = http.GET();
  if (code == 200) return true;
  Serial.printf("GET %s -> %d\n", url, code);
  http.end();
  return false;
}

// ---------- Aladhan fallback ----------
bool fetchDay(uint32_t ymd, int city, Day& out) {
  char url[320];
  snprintf(url, sizeof url,
           "https://api.aladhan.com/v1/timings/%02d-%02d-%04d?latitude=%.4f&longitude=%.4f&method=%d"
           "&timezonestring=%s&tune=0,%d,%d,%d,%d,%d,0,%d,0",
           (int)(ymd % 100), (int)(ymd / 100 % 100), (int)(ymd / 10000), CITIES[city].lat, CITIES[city].lon, METHOD,
           TIMEZONE, TUNE_FAJR, TUNE_SUNRISE, TUNE_DHUHR, TUNE_ASR, TUNE_MAGHRIB, TUNE_ISHA);
  WiFiClientSecure c;
  HTTPClient http;
  if (!httpBegin(http, c, url)) return false;
  JsonDocument filter, doc;
  for (const char* k : PRAYER_KEY) filter["data"]["timings"][k] = true;
  filter["data"]["date"]["hijri"]["day"] = true;
  filter["data"]["date"]["hijri"]["month"]["number"] = true;
  filter["data"]["date"]["hijri"]["year"] = true;
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) { Serial.printf("aladhan: %s\n", err.c_str()); return false; }
  JsonObject times = doc["data"]["timings"];
  out.ymd = ymd;
  out.fromSite = 0;
  for (int i = 0; i < 6; i++) {
    int m = parseHM(times[PRAYER_KEY[i]] | "");
    if (m < 0) { Serial.println("aladhan: bad times"); return false; }
    out.mins[i] = m;
  }
  JsonObject h = doc["data"]["date"]["hijri"];
  copyUtf8(out.hijri, sizeof out.hijri,
           hijriFromNumbers(atoi(h["day"] | "0"), h["month"]["number"] | 0, atoi(h["year"] | "0")).c_str());
  return true;
}

// Fills missing days (today + min(0, HIJRI_ADJUST) .. today + 6 + max(0, HIJRI_ADJUST)), one fetch per call.
// Returns 1 fetched, 0 nothing missing, -1 failed.
int syncAladhan(time_t now) {
  int lo = min(0, HIJRI_ADJUST), hi = 6 + max(0, HIJRI_ADJUST);
  uint32_t want = 0;
  int city;
  {
    Guard g;
    uint32_t first = ymdPlus(now, lo);
    int k = 0;
    for (int i = 0; i < S.nDays; i++)
      if (S.days[i].ymd >= first) S.days[k++] = S.days[i];
    if (k != S.nDays) { S.nDays = k; saveDays(); }
    city = S.city;
    for (int d = lo; d <= hi && !want; d++) {
      uint32_t y = ymdPlus(now, d);
      if (!findDay(S, y)) want = y;
    }
  }
  if (!want) return 0;
  Day day;
  if (!fetchDay(want, city, day)) return -1;
  Guard g;
  if (S.city == city && S.nDays < MAX_DAYS && !findDay(S, want)) {
    S.days[S.nDays++] = day;
    S.prayerSync = now;
    saveDays();
  }
  return 1;
}

// ---------- news ----------
bool syncNews(time_t now) {
  static News all[N_FEEDS * ITEMS_PER_FEED];
  int n = 0, okFeeds = 0;
  for (int f = 0; f < N_FEEDS; f++) {
    esp_task_wdt_reset();
    WiFiClientSecure c;
    HTTPClient http;
    if (!httpBegin(http, c, FEEDS[f].url)) continue;
    RssParser p(ITEMS_PER_FEED);
    uint8_t buf[256];
    uint32_t t0 = millis();
    while (!p.done() && millis() - t0 < 20000) {
      int avail = c.available();
      if (avail <= 0) {
        if (!c.connected()) break;
        delay(5);
        continue;
      }
      int r = c.read(buf, min(avail, (int)sizeof buf));
      for (int i = 0; i < r && !p.done(); i++) p.feed((char)buf[i]);
      esp_task_wdt_reset();
    }
    http.end();
    if (p.n) okFeeds++;
    else Serial.printf("rss: no items from %s\n", FEEDS[f].url);
    for (int i = 0; i < p.n && n < N_FEEDS * ITEMS_PER_FEED; i++, n++) {
      copyUtf8(all[n].title, sizeof all[n].title, stripMarks(p.items[i].title.c_str()).c_str());
      copyUtf8(all[n].src, sizeof all[n].src, FEEDS[f].name);
      all[n].when = p.items[i].when;
    }
  }
  Guard g;
  S.newsFailed = !okFeeds;
  if (!okFeeds) return false;  // keep the cached list
  std::sort(all, all + n, [](const News& a, const News& b) { return a.when > b.when; });
  S.nNews = min(n, MAX_NEWS);
  memcpy(S.news, all, sizeof(News) * S.nNews);
  S.newsSync = now;
  S.newsVer++;
  return true;
}

// ---------- Telegram ----------
WiFiClientSecure tgClient;
UniversalTelegramBot bot(TELEGRAM_TOKEN, tgClient);

bool botSendOk(const String& text) {
  if (!S.botEnabled) return false;
  bool ok = bot.sendMessage(ALLOWED_CHAT_ID, text, "");
  if (!ok) Serial.println("bot: send failed");
  return ok;
}
void botSend(const String& text) { botSendOk(text); }

String sinceText(time_t now, time_t t) { return t ? ageAr(now, t) : String("لم يتم بعد"); }

String todayText(const State& s, time_t now) {
  const Day* d = findDay(s, ymdPlus(now, 0));
  if (!d) return "لا توجد مواقيت بعد، جاري التحميل.";
  tm t;
  localtime_r(&now, &t);
  String o = "🕌 مواقيت اليوم - " + String(CITIES[s.city].nameAr) + "\n" + gregorianAr(t, true) + "\n" +
             hijriFor(s, now) + "\n\n";
  for (int i = 0; i < 6; i++) o += String(PRAYER_AR[i]) + ": " + fmt24(d->mins[i]) + "\n";
  return o;
}

String nextText(const State& s, time_t now) {
  const Day* d = findDay(s, ymdPlus(now, 0));
  if (!d) return "لا توجد مواقيت بعد.";
  const Day* tmr = findDay(s, ymdPlus(now, 1));
  tm t;
  localtime_r(&now, &t);
  long nowSec = t.tm_hour * 3600L + t.tm_min * 60 + t.tm_sec;
  Span sp = currentSpan(d->mins, tmr ? tmr->mins : nullptr, nowSec);
  return "🕌 الصلاة القادمة: " + String(PRAYER_AR[sp.next]) + " " + fmt24(sp.end / 60) + "\nبعد " + fmtDuration(sp.end - nowSec);
}

String newsText(const State& s, time_t now, int max) {
  if (!s.nNews) return "لا توجد أخبار بعد.";
  String o = "📰 آخر الأخبار\n";
  for (int i = 0; i < s.nNews && i < max; i++)
    o += "\n• " + String(s.news[i].title) + "\n   " + s.news[i].src + " · " + ageAr(now, s.news[i].when) + "\n";
  return o;
}

String statusText(const State& s, time_t now) {
  return "⚙️ الحالة\nالشبكة: " + String(s.ssid) + " (" + s.rssi + " dBm)\nIP: " + s.ip + "\nالمدينة: " +
         CITIES[s.city].nameAr + "\nالمواقيت: " + sinceText(now, s.prayerSync) + " (" + s.nDays + " أيام مخزنة)" +
         "\nموقع نور اليقين: " + (!s.siteEnabled ? "غير مفعّل" : s.siteFailed ? "لا يستجيب" : "متصل") +
         "\nالأخبار: " + sinceText(now, s.newsSync) + "\nالتنبيهات: " +
         (s.muteUntil > now ? "صامتة حتى " + fmt24(localMins(s.muteUntil)) : String("مفعّلة")) +
         "\nمدة التشغيل: " + uptimeAr(millis() / 1000) + "\nالذاكرة الحرة: " + (ESP.getFreeHeap() / 1024) + " KB";
}

String helpText() {
  return "أوامر الساعة:\n/today مواقيت اليوم\n/next الصلاة القادمة\n/news آخر الأخبار\n/status الحالة\n"
         "/mute N كتم التنبيهات N دقيقة (0 للإلغاء)\n/city beirut أو /city saida";
}

void handleMessage(const telegramMessage& m, time_t now) {
  if (m.chat_id != ALLOWED_CHAT_ID) { Serial.println("bot: ignored a message from another chat"); return; }
  String t = m.text, cmd, arg;
  t.trim();
  int sp = t.indexOf(' ');
  cmd = sp > 0 ? t.substring(0, sp) : t;
  if (sp > 0) { arg = t.substring(sp + 1); arg.trim(); }
  int at = cmd.indexOf('@');  // "/today@MyBot" in groups
  if (at > 0) cmd.remove(at);
  static State s;
  { Guard g; s = S; }
  if (cmd == "/today") botSend(todayText(s, now));
  else if (cmd == "/next") botSend(nextText(s, now));
  else if (cmd == "/news") botSend(newsText(s, now, 4));
  else if (cmd == "/status") botSend(statusText(s, now));
  else if (cmd == "/mute") {
    long n = constrain(arg.toInt(), 0L, 7L * 24 * 60);
    { Guard g; S.muteUntil = n > 0 ? now + n * 60 : 0; prefs.putLong("mute", S.muteUntil); }
    botSend(n > 0 ? "🔕 تم كتم التنبيهات لمدة " + countAr(n, "دقيقة", "دقيقتين", "دقائق") : String("🔔 تم إلغاء الكتم"));
  } else if (cmd == "/city") {
    arg.toLowerCase();
    int c = -1;
    for (int i = 0; i < N_CITIES; i++)
      if (arg == CITIES[i].key) c = i;
    if (c < 0) { botSend("الاستخدام: /city beirut أو /city saida"); return; }
    { Guard g; S.city = c; S.nDays = 0; prefs.putInt("city", c); saveDays(); }
    sitePrayerYmd = 0;
    siteDays.ok(0);
    aladhanTry.ok(0);
    botSend("📍 تم التبديل إلى " + String(CITIES[c].nameAr) + "، جاري تحديث المواقيت.");
  } else botSend(helpText());
}

void pollBot(time_t now) {
  static uint32_t lastPoll = 0, lastMe = 0;
  if (!S.botEnabled || millis() - lastPoll < 3000) return;
  lastPoll = millis();
  if (!lastMe || millis() - lastMe > 5UL * 60 * 1000) {  // getMe is the only call that reports failure clearly
    bool ok = bot.getMe();
    lastMe = millis();
    Guard g;
    S.botOk = ok;
  }
  int n = bot.getUpdates(bot.last_message_received + 1);
  if (n > 0) { Guard g; S.botOk = true; }
  for (int i = 0; i < n; i++) handleMessage(bot.messages[i], now);
}

// Reminders 10 min before each prayer, at the adhan, and the morning digest. Each fires once per day,
// only inside a 2-minute window, so a reboot never floods old messages.
void checkEvents(time_t now) {
  static uint32_t sent[3][6] = {};
  static time_t last = 0;
  if (now == last) return;
  last = now;
  static State s;
  { Guard g; s = S; }
  tm t;
  localtime_r(&now, &t);
  uint32_t ymd = ymdOf(t);
  if (s.adhanYmd != ymd) { Guard g; S.adhanYmd = ymd; memset(S.adhanSent, 0, sizeof S.adhanSent); }
  int nowMin = t.tm_hour * 60 + t.tm_min;
  const Day* d = findDay(s, ymd);
  if (!d || !s.botEnabled) return;
  bool muted = s.muteUntil > now;
  auto due = [&](int kind, int i, int atMin) {
    if (nowMin < atMin || nowMin >= atMin + 2 || sent[kind][i] == ymd) return false;
    sent[kind][i] = ymd;
    return !muted;
  };
  for (int i : PRAYERS) {
    if (due(0, i, d->mins[i] - REMIND_BEFORE_MIN))
      botSend("⏰ " + String(PRAYER_AR[i]) + " بعد " + countAr(REMIND_BEFORE_MIN, "دقيقة", "دقيقتين", "دقائق") + " (" +
              fmt24(d->mins[i]) + ")");
    if (due(1, i, d->mins[i])) {
      bool ok = botSendOk("🕌 حان الآن موعد صلاة " + String(PRAYER_AR[i]) + " – " + fmt24(d->mins[i]));
      Guard g;
      S.adhanSent[i] = ok ? 1 : -1;
    }
  }
  if (due(2, 0, d->mins[0] + DIGEST_AFTER_FAJR_MIN)) botSend("☀️ صباح الخير\n\n" + todayText(s, now) + "\n" + newsText(s, now, 3));
}

// ---------- website schedule: one call per loop pass, most urgent first ----------
void syncSite(time_t now) {
  static uint32_t lastNoorMs = 0, noor8Ymd = 0, adhkarYmd = 0, lastTbMs = 0;
  uint32_t today = ymdPlus(now, 0);
  int nowMin = localMins(now);
  Tasbeeh tb;
  { Guard g; tb = S.tb; }
  bool tbWaiting = tb.outDelta || tb.pending[0] || tb.pending[1] || tb.pending[2];

  if (sitePrayerYmd != today && (!sitePrayerYmd || nowMin >= PRAYER_FETCH_AFTER_MIN) && siteDays.due()) {
    if (noorPrayer(now)) { sitePrayerYmd = today; siteDays.ok(0); }
    else siteDays.fail();
  } else if (tbWaiting && (tb.sinceBatch >= TASBEEH_SYNC_PRESSES || !lastTbMs || millis() - lastTbMs >= TASBEEH_SYNC_MS) &&
             siteTasbeeh.due()) {
    if (noorTasbeeh(now)) { lastTbMs = millis(); siteTasbeeh.ok(0); }
    else siteTasbeeh.fail();
  } else if (siteBeat.due()) {
    if (noorHeartbeat(now)) siteBeat.ok(HEARTBEAT_MS);
    else siteBeat.fail();
  } else if ((!lastNoorMs || millis() - lastNoorMs >= NOOR_EVERY_MS || (nowMin >= NOOR_FETCH_AT_MIN && noor8Ymd != today)) &&
             siteNoor.due()) {
    if (noorToday(now)) {
      lastNoorMs = millis();
      if (nowMin >= NOOR_FETCH_AT_MIN) noor8Ymd = today;
      siteNoor.ok(60000);
    } else siteNoor.fail();
  } else if (adhkarYmd != today && siteAdhkar.due()) {
    if (noorAdhkar(0, today) && noorAdhkar(1, today)) { adhkarYmd = today; siteAdhkar.ok(0); }
    else siteAdhkar.fail();
  }
}

// ---------- the task ----------
void netTask(void*) {
  WiFi.setHostname(HOSTNAME);
  WiFiManager wm;
  wm.setConfigPortalBlocking(false);
  wm.setConnectTimeout(20);
  wm.setHostname(HOSTNAME);
  if (!wm.autoConnect(PORTAL_SSID, PORTAL_PASSWORD)) Serial.printf("wifi: setup portal open (%s)\n", PORTAL_SSID);
  configTzTime(TZ_POSIX, "pool.ntp.org", "time.google.com");
  tgClient.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  esp_task_wdt_add(nullptr);
  bool otaUp = false;
  for (;;) {
    esp_task_wdt_reset();
    wm.process();
    bool up = WiFi.status() == WL_CONNECTED;
    {
      Guard g;
      S.wifi = up;
      S.portal = wm.getConfigPortalActive();
      S.rssi = up ? WiFi.RSSI() : 0;
      copyUtf8(S.ip, sizeof S.ip, up ? WiFi.localIP().toString().c_str() : "-");
      copyUtf8(S.ssid, sizeof S.ssid, up ? WiFi.SSID().c_str() : "-");
    }
    if (up && !otaUp) {
      ArduinoOTA.setHostname(HOSTNAME);
      ArduinoOTA.setPassword(OTA_PASSWORD);
      ArduinoOTA.begin();
      otaUp = true;
    }
    if (otaUp) ArduinoOTA.handle();
    time_t now = time(nullptr);
    if (up && now > 1700000000) {  // never act on an unsynced clock
      if (S.siteEnabled) syncSite(now);
      esp_task_wdt_reset();
      // Aladhan only while the website has not delivered today's times
      if ((!S.siteEnabled || sitePrayerYmd != ymdPlus(now, 0)) && aladhanTry.due()) {
        int r = syncAladhan(time(nullptr));
        if (r < 0) aladhanTry.fail();
        else aladhanTry.ok(r ? 500 : 60000);
      }
      if (newsTry.due()) {
        if (syncNews(time(nullptr))) newsTry.ok(NEWS_EVERY_MS);
        else newsTry.fail();
      }
      checkEvents(time(nullptr));
      pollBot(time(nullptr));
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
