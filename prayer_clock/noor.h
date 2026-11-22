#pragma once
// Noor Al-Yaqeen device API client (contract: NOOR_DEVICE_INTEGRATION.md). Runs in the network task only.
// Every call is HTTPS with the real certificate chain (certs.h) and the X-Device-Key header.

// GET or POST `path`; parses the JSON reply into `out` (through `filter` when given). Returns the HTTP code, or -1.
int noorCall(const char* method, const String& path, const String& body, JsonDocument& out, JsonDocument* filter = nullptr) {
  WiFiClientSecure c;
  c.setCACert(NOOR_ROOT_CAS);
  HTTPClient http;
  http.setConnectTimeout(10000);
  http.setTimeout(10000);
  http.useHTTP10(true);
  String url = String(NOOR_BASE_URL) + path;
  if (!http.begin(c, url)) return -1;
  http.addHeader("X-Device-Key", DEVICE_KEY);
  int code;
  if (!strcmp(method, "POST")) {
    http.addHeader("Content-Type", "application/json");
    code = http.POST(body);
  } else code = http.GET();
  if (code == 200) {
    DeserializationError e = filter ? deserializeJson(out, http.getStream(), DeserializationOption::Filter(*filter))
                                    : deserializeJson(out, http.getStream());
    if (e) { Serial.printf("noor %s: %s\n", path.c_str(), e.c_str()); code = -2; }
  } else Serial.printf("noor %s %s -> %d\n", method, path.c_str(), code);  // never log the key
  http.end();
  { Guard g; S.siteFailed = code != 200; }
  return code;
}

// GET /prayer?days=7: replaces the whole cache when it brings today. Returns true on success.
bool noorPrayer(time_t now) {
  int city;
  { Guard g; city = S.city; }
  tm t;
  localtime_r(&now, &t);
  char path[120];
  snprintf(path, sizeof path, "/prayer?lat=%.4f&lng=%.4f&date=%04d-%02d-%02d&days=7", CITIES[city].lat, CITIES[city].lon,
           t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  JsonDocument doc;
  if (noorCall("GET", path, "", doc) != 200) return false;
  static Day got[MAX_DAYS];
  int n = 0;
  for (JsonObject d : doc["days"].as<JsonArray>()) {
    if (n >= 7) break;
    Day& day = got[n];
    day.ymd = parseYmd(d["date"] | "");
    bool ok = day.ymd != 0;
    for (int i = 0; i < 6 && ok; i++) ok = (day.mins[i] = parseHM(d[PRAYER_SITE_KEY[i]] | "")) >= 0;
    if (!ok) continue;  // skip a malformed day rather than trust it
    copyUtf8(day.hijri, sizeof day.hijri, d["hijri"] | "");
    day.fromSite = 1;
    n++;
  }
  bool hasToday = false;
  for (int i = 0; i < n; i++) hasToday |= got[i].ymd == ymdOf(t);
  if (!hasToday) { Serial.println("noor prayer: reply has no entry for today"); return false; }
  Guard g;
  if (S.city != city) return false;  // switched meanwhile; fetch again
  memcpy(S.days, got, sizeof(Day) * n);
  S.nDays = n;
  S.prayerSync = now;
  saveDays();
  return true;
}

bool noorToday(time_t now) {
  JsonDocument doc;
  if (noorCall("GET", "/today", "", doc) != 200) return false;
  const char* text = doc["text"] | "";
  if (!*text) return false;
  Guard g;
  copyUtf8(noor.date, sizeof noor.date, doc["date"] | "");
  copyUtf8(noor.type, sizeof noor.type, doc["type"] | "");
  copyUtf8(noor.text, sizeof noor.text, text);
  copyUtf8(noor.source, sizeof noor.source, doc["source"] | "");
  noor.fetched = now;
  S.noorVer++;
  saveNoor();
  return true;
}

bool noorAdhkar(int set, uint32_t ymd) {
  JsonDocument doc;
  if (noorCall("GET", set ? "/adhkar?set=evening" : "/adhkar?set=morning", "", doc) != 200) return false;
  static AdhkarSet a;
  a.n = 0;
  for (JsonObject it : doc["items"].as<JsonArray>()) {
    if (a.n >= 14) break;
    const char* text = it["text"] | "";
    if (!*text) continue;
    copyUtf8(a.items[a.n].text, sizeof a.items[a.n].text, text);
    a.items[a.n].count = constrain((int)(it["count"] | 1), 1, 1000);
    a.n++;
  }
  if (!a.n) return false;
  a.ymd = ymd;
  Guard g;
  adhkar[set] = a;
  S.adhkarVer++;
  saveAdhkar(set);
  return true;
}

// Sends one batch (frozen with its seq so a lost reply is simply resent). Returns false on failure.
bool noorTasbeeh(time_t now) {
  Tasbeeh t;
  {
    Guard g;
    if (!tasbeehMakeBatch(S.tb)) return true;
    saveTasbeeh();  // the batch exists on flash before it leaves
    t = S.tb;
  }
  JsonDocument body, doc;
  body["phrase"] = TASBEEH_AR[t.outPhrase];
  body["delta"] = t.outDelta;
  body["ts"] = isoTime(now);
  body["seq"] = t.outSeq;
  String b;
  serializeJson(body, b);
  bool ok = noorCall("POST", "/tasbeeh", b, doc) == 200;
  Guard g;
  S.tb.lastSyncOk = ok;
  if (!ok) return false;
  if (S.tb.outSeq == t.outSeq) S.tb.outDelta = 0;  // landed
  if (doc["todayTotal"].is<uint32_t>()) { S.tb.serverTotal = doc["todayTotal"]; S.tb.totalYmd = ymdPlus(now, 0); }
  S.tb.lastSync = now;
  saveTasbeeh();
  return true;
}

void botSend(const String& text);
bool noorHeartbeat(time_t now) {
  JsonDocument body, doc;
  {
    Guard g;
    body["fw"] = FW_VERSION;
    body["rssi"] = S.rssi;
    body["ip"] = S.ip;
    body["uptime"] = millis() / 1000;
    body["lastPrayerSync"] = isoTime(S.prayerSync);
    body["lastNewsSync"] = isoTime(S.newsSync);
  }
  String b;
  serializeJson(body, b);
  if (noorCall("POST", "/heartbeat", b, doc) != 200) return false;
  for (JsonObject m : doc["messages"].as<JsonArray>()) {
    const char* id = m["id"] | "";
    const char* text = m["text"] | "";
    if (!*id || !*text) continue;
    bool fresh;
    {
      Guard g;
      fresh = strncmp(S.banner.id, id, sizeof S.banner.id - 1) != 0;  // an unacked message comes back: show, don't resend
      copyUtf8(S.banner.id, sizeof S.banner.id, id);
      copyUtf8(S.banner.text, sizeof S.banner.text, text);
      time_t until = parseIso(m["until"] | "");
      S.banner.until = until ? until : now + 3600;  // shortcut: no `until` shows it for an hour
    }
    if (fresh) botSend(String("📩 رسالة على الشاشة:\n") + text);
    JsonDocument ack, ackReply;
    ack["id"] = id;
    String a;
    serializeJson(ack, a);
    noorCall("POST", "/ack", a, ackReply);  // a lost ack only means the message comes back next minute
  }
  return true;
}
