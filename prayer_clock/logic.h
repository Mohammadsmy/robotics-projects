#pragma once
// Pure helpers (no hardware, no network, no LVGL). selfTest() checks them at every boot.
#include "arabic_forms.h"

const char* const PRAYER_AR[6] = {"الفجر", "الشروق", "الظهر", "العصر", "المغرب", "العشاء"};
const char* const PRAYER_KEY[6] = {"Fajr", "Sunrise", "Dhuhr", "Asr", "Maghrib", "Isha"};
const char* const PRAYER_SITE_KEY[6] = {"fajr", "sunrise", "dhuhr", "asr", "maghrib", "isha"};
const int PRAYERS[5] = {0, 2, 3, 4, 5};  // Sunrise is shown but is not a prayer
const char* const WEEKDAY_AR[7] = {"الأحد", "الإثنين", "الثلاثاء", "الأربعاء", "الخميس", "الجمعة", "السبت"};
const char* const MONTH_AR[12] = {"كانون الثاني", "شباط", "آذار", "نيسان", "أيار", "حزيران",
                                  "تموز", "آب", "أيلول", "تشرين الأول", "تشرين الثاني", "كانون الأول"};
const char* const HIJRI_AR[12] = {"محرم", "صفر", "ربيع الأول", "ربيع الآخر", "جمادى الأولى", "جمادى الآخرة",
                                  "رجب", "شعبان", "رمضان", "شوال", "ذو القعدة", "ذو الحجة"};
const char* const TASBEEH_AR[3] = {"سبحان الله", "الحمد لله", "الله أكبر"};

uint32_t ymdOf(const tm& t) { return (t.tm_year + 1900) * 10000 + (t.tm_mon + 1) * 100 + t.tm_mday; }

// Local date `days` away from now; noon arithmetic keeps DST changes out of it.
uint32_t ymdPlus(time_t now, int days) {
  tm t;
  localtime_r(&now, &t);
  t.tm_hour = 12;
  t.tm_min = t.tm_sec = 0;
  t.tm_mday += days;
  t.tm_isdst = -1;
  mktime(&t);
  return ymdOf(t);
}

int localMins(time_t t) {
  tm x;
  localtime_r(&t, &x);
  return x.tm_hour * 60 + x.tm_min;
}

// "04:40" or "04:40 (EET)" -> minutes after midnight, or -1.
int parseHM(const char* s) {
  if (!s || strlen(s) < 5 || s[2] != ':') return -1;
  for (int i : {0, 1, 3, 4})
    if (!isdigit((unsigned char)s[i])) return -1;
  int h = (s[0] - '0') * 10 + s[1] - '0', m = (s[3] - '0') * 10 + s[4] - '0';
  return h < 24 && m < 60 ? h * 60 + m : -1;
}

// "2026-11-19" -> 20261119, or 0.
uint32_t parseYmd(const char* s) {
  int y, m, d;
  if (!s || sscanf(s, "%4d-%2d-%2d", &y, &m, &d) != 3 || m < 1 || m > 12 || d < 1 || d > 31) return 0;
  return y * 10000 + m * 100 + d;
}

// From the last prayer to the next one, in seconds of today. After Isha the next is tomorrow's Fajr.
struct Span { int prev, next; long start, end; };
Span currentSpan(const int16_t* m, const int16_t* tomorrow, long nowSec) {
  for (int k = 0; k < 5; k++)
    if (m[PRAYERS[k]] * 60L > nowSec)
      return {k ? PRAYERS[k - 1] : 5, PRAYERS[k], k ? m[PRAYERS[k - 1]] * 60L : m[5] * 60L - 86400, m[PRAYERS[k]] * 60L};
  // shortcut: without tomorrow's entry, today's Fajr stands in (1-2 min off); the 7-day cache normally has it.
  return {5, 0, m[5] * 60L, (tomorrow ? tomorrow[0] : m[0]) * 60L + 86400};
}

String fmt24(int mins) {
  char b[8];
  snprintf(b, sizeof b, "%02d:%02d", mins / 60 % 24, mins % 60);
  return b;
}

String fmtClock(int h, int m) {
  char b[8];
  if (CLOCK_12H) snprintf(b, sizeof b, "%d:%02d", h % 12 ? h % 12 : 12, m);
  else snprintf(b, sizeof b, "%02d:%02d", h, m);
  return b;
}

String fmtDuration(long s) {
  char b[16];
  if (s >= 3600) snprintf(b, sizeof b, "%ld:%02ld:%02ld", s / 3600, s / 60 % 60, s % 60);
  else snprintf(b, sizeof b, "%ld:%02ld", s / 60, s % 60);
  return b;
}

// Home countdown, per minute: "2:26", or "45 د" under an hour.
String countdownAr(long s) {
  long m = (s + 59) / 60;
  char b[16];
  if (m >= 60) snprintf(b, sizeof b, "%ld:%02ld", m / 60, m % 60);
  else snprintf(b, sizeof b, "%ld د", m);
  return b;
}

String gregorianAr(const tm& t, bool year = false) {
  String s = String(WEEKDAY_AR[t.tm_wday]) + " " + t.tm_mday + " " + MONTH_AR[t.tm_mon];
  return year ? s + " " + (t.tm_year + 1900) : s;
}

String hijriFromNumbers(int d, int m, int y) {
  if (m < 1 || m > 12) return "";
  return String(d) + " " + HIJRI_AR[m - 1] + " " + y;
}

// Arabic count + unit: 1 دقيقة, 2 دقيقتين, 3-10 دقائق, 11+ دقيقة
String countAr(long n, const char* one, const char* two, const char* few) {
  if (n == 1) return one;
  if (n == 2) return two;
  return String(n) + " " + (n <= 10 ? few : one);
}

String ageAr(time_t now, time_t when) {
  if (!when) return "";
  long s = now - when;
  if (s < 60) return "الآن";
  if (s < 3600) return "قبل " + countAr(s / 60, "دقيقة", "دقيقتين", "دقائق");
  if (s < 86400) return "قبل " + countAr(s / 3600, "ساعة", "ساعتين", "ساعات");
  return "قبل " + countAr(s / 86400, "يوم", "يومين", "أيام");
}

String uptimeAr(uint32_t s) {
  if (s < 3600) return countAr(max<uint32_t>(s / 60, 1), "دقيقة", "دقيقتين", "دقائق");
  if (s < 86400) return countAr(s / 3600, "ساعة", "ساعتين", "ساعات");
  return countAr(s / 86400, "يوم", "يومين", "أيام") + " " + countAr(s / 3600 % 24, "ساعة", "ساعتين", "ساعات");
}

const char* signalAr(int rssi) { return rssi >= -60 ? "إشارة ممتازة" : rssi >= -72 ? "إشارة جيدة" : "إشارة ضعيفة"; }

// Days since 1970-01-01 (Howard Hinnant's days_from_civil).
long daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  long era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long)doe - 719468;
}

// RSS pubDate "Thu, 19 Nov 2026 06:17:06 GMT" or "... +0300" -> UTC epoch, or 0 if unreadable.
time_t parseRfc822(const char* s) {
  const char* comma = strchr(s, ',');
  if (comma) s = comma + 1;
  int d, y, hh, mm, ss = 0;
  char mon[4] = {0}, zone[8] = {0};
  if (sscanf(s, " %d %3s %d %d:%d:%d %7s", &d, mon, &y, &hh, &mm, &ss, zone) < 5) return 0;
  const char* M = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char* p = strstr(M, mon);
  if (!p || strlen(mon) != 3 || (p - M) % 3) return 0;
  if (y < 100) y += 2000;
  long off = 0;
  if ((zone[0] == '+' || zone[0] == '-') && strlen(zone) >= 5) {
    int z = atoi(zone + 1);
    off = (z / 100 * 3600L + z % 100 * 60) * (zone[0] == '-' ? -1 : 1);
  }
  return (time_t)(daysFromCivil(y, (p - M) / 3 + 1, d) * 86400L + hh * 3600L + mm * 60 + ss - off);
}

// ISO 8601 "2026-11-21T10:00:00Z", "...00.000Z" or "...00+02:00" -> UTC epoch, or 0.
time_t parseIso(const char* s) {
  int y, mo, d, h, mi, se = 0, n = 0;
  if (!s || sscanf(s, "%4d-%2d-%2dT%2d:%2d%n", &y, &mo, &d, &h, &mi, &n) < 5) return 0;
  const char* p = s + n;
  if (*p == ':') { se = atoi(p + 1); p += 3; }
  while (*p == '.' || isdigit((unsigned char)*p)) p++;
  long off = 0;
  if (*p == '+' || *p == '-') {
    int zh = 0, zm = 0;
    sscanf(p + 1, "%2d:%2d", &zh, &zm);
    off = (zh * 3600L + zm * 60) * (*p == '-' ? -1 : 1);
  }
  return (time_t)(daysFromCivil(y, mo, d) * 86400L + h * 3600L + mi * 60 + se - off);
}

// Local time as ISO 8601 with offset, "2026-11-21T10:00:00+02:00".
String isoTime(time_t t) {
  if (!t) return "";
  tm x;
  localtime_r(&t, &x);
  char b[32];
  strftime(b, sizeof b, "%Y-%m-%dT%H:%M:%S%z", &x);  // ...+0200
  String s = b;
  if (s.length() == 24) s = s.substring(0, 22) + ":" + s.substring(22);
  return s;
}

// Copy at most cap-1 bytes without cutting a UTF-8 character in half.
void copyUtf8(char* dst, size_t cap, const char* src) {
  size_t n = strlen(src);
  if (n >= cap) {
    n = cap - 1;
    while (n && (src[n] & 0xC0) == 0x80) n--;
  }
  memcpy(dst, src, n);
  dst[n] = 0;
}

void appendUtf8(String& o, uint32_t cp) {
  if (cp < 0x80) o += (char)cp;
  else if (cp < 0x800) { o += (char)(0xC0 | cp >> 6); o += (char)(0x80 | (cp & 0x3F)); }
  else if (cp < 0x10000) { o += (char)(0xE0 | cp >> 12); o += (char)(0x80 | (cp >> 6 & 0x3F)); o += (char)(0x80 | (cp & 0x3F)); }
  else { o += (char)(0xF0 | cp >> 18); o += (char)(0x80 | (cp >> 12 & 0x3F)); o += (char)(0x80 | (cp >> 6 & 0x3F)); o += (char)(0x80 | (cp & 0x3F)); }
}

// Next code point of a UTF-8 string; advances i.
uint32_t nextUtf8(const char* s, size_t& i) {
  uint8_t c = s[i++];
  if (c < 0x80) return c;
  int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : 1;
  uint32_t cp = c & (0x3F >> extra);
  while (extra-- && (s[i] & 0xC0) == 0x80) cp = cp << 6 | (s[i++] & 0x3F);
  return cp;
}

// Decode XML entities, strip tags left inside text, collapse whitespace.
String cleanText(const String& in) {
  String o;
  o.reserve(in.length());
  bool space = false, tag = false;
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if (tag) { if (c == '>') tag = false; continue; }
    if (c == '<') { tag = true; continue; }
    if (c == '&') {
      int semi = in.indexOf(';', i);
      if (semi > 0 && semi - (int)i <= 9) {
        String e = in.substring(i + 1, semi);
        uint32_t cp = e == "amp" ? '&' : e == "lt" ? '<' : e == "gt" ? '>' : e == "quot" ? '"' : e == "apos" ? '\'' : e == "nbsp" ? ' ' : 0;
        if (!cp && e.length() > 1 && e[0] == '#')
          cp = (e[1] == 'x' || e[1] == 'X') ? strtoul(e.c_str() + 2, nullptr, 16) : strtoul(e.c_str() + 1, nullptr, 10);
        if (cp) {
          if (cp == ' ') { space = o.length() > 0; i = semi; continue; }
          if (space) o += ' ';
          space = false;
          appendUtf8(o, cp);
          i = semi;
          continue;
        }
      }
    }
    if (c == ' ' || c == '\n' || c == '\r' || c == '\t') { space = o.length() > 0; continue; }
    if (space) o += ' ';
    space = false;
    o += c;
  }
  return o;
}

// ---------- Arabic shaping ----------
// LVGL's own shaper skips only one vowel mark when joining letters, so shadda + vowel or Quranic marks break the
// word apart. This one skips any number of marks and emits presentation forms, which LVGL then only reorders.
bool arTransparent(uint32_t c) {
  return (c >= 0x064B && c <= 0x065F) || c == 0x0670 || (c >= 0x06D6 && c <= 0x06DC) || (c >= 0x06DF && c <= 0x06E4) ||
         c == 0x06E7 || c == 0x06E8 || (c >= 0x06EA && c <= 0x06ED);
}

const uint16_t* arForms(uint32_t c) {
  for (const auto& f : AR_FORMS)
    if (f[0] == c) return f;
  return nullptr;
}

bool arJoinsNext(uint32_t c) {  // dual-joining letters and tatweel connect to the following letter
  const uint16_t* f = arForms(c);
  return c == 0x0640 || (f && f[3]);
}

bool arJoinsPrev(uint32_t c) { return c == 0x0640 || (arForms(c) && arForms(c)[2]); }

uint32_t lamAlef(uint32_t alef, bool final) {
  switch (alef) {
    case 0x0622: return final ? 0xFEF6 : 0xFEF5;
    case 0x0623: return final ? 0xFEF8 : 0xFEF7;
    case 0x0625: return final ? 0xFEFA : 0xFEF9;
    case 0x0627: return final ? 0xFEFC : 0xFEFB;
  }
  return 0;
}

String shapeArabic(const char* s) {
  static uint32_t cp[1200];
  int n = 0;
  for (size_t i = 0; s[i] && n < 1200;) cp[n++] = nextUtf8(s, i);
  String out;
  out.reserve(strlen(s) + 8);
  for (int i = 0; i < n; i++) {
    uint32_t c = cp[i];
    const uint16_t* f = arForms(c);
    if (!f) { appendUtf8(out, c); continue; }
    int p = i - 1, q = i + 1;
    while (p >= 0 && arTransparent(cp[p])) p--;
    while (q < n && arTransparent(cp[q])) q++;
    bool prev = p >= 0 && arJoinsNext(cp[p]);
    if (c == 0x0644 && q < n && lamAlef(cp[q], false)) {  // lam + alef ligature
      appendUtf8(out, lamAlef(cp[q], prev));
      for (int k = i + 1; k < q; k++) appendUtf8(out, cp[k]);  // marks on the lam
      i = q;
      continue;
    }
    bool next = f[3] && q < n && arJoinsPrev(cp[q]);
    if (!f[2]) prev = false;  // a letter without a final form never joins backwards (hamza)
    uint32_t g = prev && next && f[4] ? f[4] : prev && f[2] ? f[2] : next && f[3] ? f[3] : f[1] ? f[1] : c;
    appendUtf8(out, g);
  }
  return out;
}

String stripMarks(const char* s) {  // for headlines: marks add nothing at 14-17 px
  String out;
  for (size_t i = 0; s[i];) {
    uint32_t c = nextUtf8(s, i);
    if (!arTransparent(c)) appendUtf8(out, c);
  }
  return out;
}

// ---------- BOOT button: short / double / long press ----------
enum BtnEvent { BTN_NONE, BTN_SHORT, BTN_DOUBLE, BTN_LONG };
struct Button {
  bool down = false, longFired = false, waiting = false;
  uint32_t downAt = 0, upAt = 0;
  // pressed = debounced level; wantDouble delays a single press by 400 ms to tell it from a double press.
  BtnEvent update(bool pressed, uint32_t now, bool wantDouble) {
    if (pressed && !down) { down = true; downAt = now; longFired = false; return BTN_NONE; }
    if (pressed && !longFired && now - downAt >= 2000) { longFired = true; waiting = false; return BTN_LONG; }
    if (!pressed && down) {
      down = false;
      if (longFired) return BTN_NONE;
      if (!wantDouble) return BTN_SHORT;
      if (waiting) { waiting = false; return BTN_DOUBLE; }
      waiting = true;
      upAt = now;
      return BTN_NONE;
    }
    if (waiting && !down && now - upAt >= 400) { waiting = false; return BTN_SHORT; }
    return BTN_NONE;
  }
};

// ---------- tasbeeh: 33 x (سبحان الله، الحمد لله، الله أكبر) ----------
void tasbeehPress(Tasbeeh& t) {
  t.pending[t.phrase]++;
  t.sinceBatch++;
  if (++t.count >= TASBEEH_CYCLE) { t.count = 0; t.phrase = (t.phrase + 1) % 3; }
}

// Freeze the largest pending phrase into the outgoing batch. Returns false when nothing is waiting.
bool tasbeehMakeBatch(Tasbeeh& t) {
  if (t.outDelta) return true;  // the previous batch has not landed: resend it as is
  int best = 0;
  for (int i = 1; i < 3; i++)
    if (t.pending[i] > t.pending[best]) best = i;
  if (!t.pending[best]) return false;
  t.outPhrase = best;
  t.outDelta = t.pending[best];
  t.outSeq = ++t.nextSeq;
  t.pending[best] = 0;
  t.sinceBatch = 0;
  return true;
}

uint32_t tasbeehToday(const Tasbeeh& t, uint32_t ymd) {
  return (t.totalYmd == ymd ? t.serverTotal : 0) + t.outDelta + t.pending[0] + t.pending[1] + t.pending[2];
}

// ---------- the sun's path on Home ----------
// t = 0 at sunrise (x 400) .. 1 at maghrib (x 56), peak at x 228, y 8; horizon y 60 (DESIGN.md).
float sunX(float t) { return 228 + 172 * cosf(PI * t); }
float sunY(float t) { return 60 - 52 * sinf(PI * t); }
