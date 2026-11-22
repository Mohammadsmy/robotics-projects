#pragma once
// Data shared by the UI task (core 1) and the network task (core 0).
// Touch S, noor, adhkar and prefs only while holding a Guard.

struct Day { uint32_t ymd; int16_t mins[6]; char hijri[44]; uint8_t fromSite; };
struct News { char title[240]; char src[24]; time_t when; };
struct Banner { char id[24]; char text[280]; time_t until; };
struct Tasbeeh {
  uint8_t phrase, count;      // current cycle position
  uint32_t pending[3];        // presses not sent yet, per phrase
  uint32_t outSeq, outDelta;  // batch in flight (outDelta 0 = none); resent with the same seq until it lands
  uint8_t outPhrase;
  uint32_t nextSeq;
  uint32_t serverTotal, totalYmd;  // today's total as the website last reported it
  time_t lastSync;
  bool lastSyncOk;
  uint16_t sinceBatch;        // presses since the last batch was made
};
struct Noor { char date[11]; char type[8]; char text[1100]; char source[96]; time_t fetched; };
struct Dhikr { char text[360]; uint16_t count; };
struct AdhkarSet { Dhikr items[14]; int n; uint32_t ymd; };

const int MAX_DAYS = 7 + (HIJRI_ADJUST < 0 ? -HIJRI_ADJUST : HIJRI_ADJUST);
const int MAX_NEWS = 9;

struct State {
  Day days[MAX_DAYS];
  int nDays;
  News news[MAX_NEWS];
  int nNews;
  bool newsFailed;
  int city;
  time_t prayerSync, newsSync, muteUntil;
  bool wifi, portal, botOk, botEnabled, siteEnabled, siteFailed;
  int rssi;
  char ip[16], ssid[33];
  uint32_t newsVer, noorVer, adhkarVer;
  Banner banner;
  Tasbeeh tb;
  int8_t adhanSent[6];  // Telegram adhan message today: 1 sent, -1 failed, 0 not yet
  uint32_t adhanYmd;
};

State S;
Noor noor;
AdhkarSet adhkar[2];  // 0 morning, 1 evening
Preferences prefs;
SemaphoreHandle_t lock;
struct Guard {
  Guard() { xSemaphoreTake(lock, portMAX_DELAY); }
  ~Guard() { xSemaphoreGive(lock); }
};

const Day* findDay(const State& s, uint32_t ymd) {
  for (int i = 0; i < s.nDays; i++)
    if (s.days[i].ymd == ymd) return &s.days[i];
  return nullptr;
}

// ---- persistence (call while holding the Guard) ----
void saveDays() { prefs.putBytes("days", S.days, sizeof(Day) * S.nDays); prefs.putInt("ndays", S.nDays); }
void saveTasbeeh() { prefs.putBytes("tb", &S.tb, sizeof(Tasbeeh)); }
void saveNoor() { prefs.putBytes("noor", &noor, sizeof(Noor)); }
void saveAdhkar(int i) { prefs.putBytes(i ? "adhE" : "adhM", &adhkar[i], sizeof(AdhkarSet)); }

void loadPersisted() {
  prefs.begin("pclock", false);
  S.city = constrain(prefs.getInt("city", DEFAULT_CITY), 0, N_CITIES - 1);
  S.muteUntil = prefs.getLong("mute", 0);
  int n = prefs.getInt("ndays", 0);
  if (n > 0 && n <= MAX_DAYS && prefs.getBytesLength("days") == sizeof(Day) * n) {
    prefs.getBytes("days", S.days, sizeof(Day) * n);
    S.nDays = n;
  }
  if (prefs.getBytesLength("tb") == sizeof(Tasbeeh)) prefs.getBytes("tb", &S.tb, sizeof(Tasbeeh));
  if (S.tb.phrase > 2 || S.tb.count >= TASBEEH_CYCLE) S.tb.phrase = S.tb.count = 0;
  if (prefs.getBytesLength("noor") == sizeof(Noor)) prefs.getBytes("noor", &noor, sizeof(Noor));
  noor.text[sizeof noor.text - 1] = noor.source[sizeof noor.source - 1] = 0;
  for (int i = 0; i < 2; i++) {
    const char* k = i ? "adhE" : "adhM";
    if (prefs.getBytesLength(k) == sizeof(AdhkarSet)) prefs.getBytes(k, &adhkar[i], sizeof(AdhkarSet));
    adhkar[i].n = constrain(adhkar[i].n, 0, 14);
  }
  S.botEnabled = strlen(TELEGRAM_TOKEN) > 30;
  S.siteEnabled = strncmp(DEVICE_KEY, "replace-", 8) != 0 && strlen(DEVICE_KEY) >= 8;
  S.noorVer = S.adhkarVer = 1;
}
