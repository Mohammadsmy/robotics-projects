#pragma once
// UI task (core 1): LVGL 9 on LovyanGFX, Arabic right-to-left, layout from DESIGN.md (480x320).
// Reads a copy of S once per second and after each button press; never touches the network.

LV_FONT_DECLARE(font_c13);
LV_FONT_DECLARE(font_c14);
LV_FONT_DECLARE(font_c17);
LV_FONT_DECLARE(font_c24);
LV_FONT_DECLARE(font_c52);
LV_FONT_DECLARE(font_clock80);
LV_FONT_DECLARE(font_count64);
LV_FONT_DECLARE(font_am40);
LV_FONT_DECLARE(font_am34);
LV_FONT_DECLARE(font_am28);

// design tokens
const uint32_t GROUND = 0x0B1426, SURFACE = 0x16223D, SURFACE_PAST = 0x0F1A2E, TRACK = 0x24314F, INK = 0xF1F4F9,
               INK2 = 0xC9D1DF, MUTED = 0x9AA6BD, PAST = 0x7D889C, ACCENT = 0xE8B64C, ON_ACCENT = 0x0B1426,
               DOT_PAST = 0x5E6A80, ALERT_BAR = 0x14203A;

LGFX lcd;
enum Scr { HOME, NOOR, NEWS, TASBEEH, STATUS_SCR, ALERT, ADHKAR, N_SCR };
lv_obj_t* scr[N_SCR];
Scr cur = HOME, beforeAlert = HOME;

// ---------- builders ----------
lv_obj_t* screen() {
  lv_obj_t* s = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(s, lv_color_hex(GROUND), 0);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
  lv_obj_set_style_base_dir(s, LV_BASE_DIR_RTL, 0);
  lv_obj_remove_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  return s;
}

lv_obj_t* panel(lv_obj_t* parent, int x, int y, int w, int h, uint32_t bg, int radius = 10) {
  lv_obj_t* b = lv_obj_create(parent);
  lv_obj_set_pos(b, x, y);
  lv_obj_set_size(b, w, h);
  lv_obj_set_style_bg_color(b, lv_color_hex(bg), 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(b, radius, 0);
  lv_obj_set_style_border_width(b, 0, 0);
  lv_obj_set_style_pad_all(b, 0, 0);
  lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  return b;
}

// Top of a label whose text should sit visually centred on `cy` (px = the font's pixel size).
int rowY(const lv_font_t* f, int px, int cy) { return cy - (f->line_height - f->base_line) + px * 3 / 10; }

lv_obj_t* label(lv_obj_t* parent, const lv_font_t* f, uint32_t color, lv_align_t align, int x, int y) {
  lv_obj_t* l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  lv_label_set_text(l, "");
  lv_obj_align(l, align, x, y);
  return l;
}

lv_obj_t* ltr(lv_obj_t* l) { lv_obj_set_style_base_dir(l, LV_BASE_DIR_LTR, 0); return l; }

void setText(lv_obj_t* l, const String& t) {
  String s = shapeArabic(t.c_str());
  if (strcmp(lv_label_get_text(l), s.c_str())) lv_label_set_text(l, s.c_str());
}

void setColor(lv_obj_t* o, uint32_t c) { lv_obj_set_style_text_color(o, lv_color_hex(c), 0); }
void setBg(lv_obj_t* o, uint32_t c) { lv_obj_set_style_bg_color(o, lv_color_hex(c), 0); }

lv_obj_t* drawing(lv_obj_t* parent, int x, int y, int w, int h, lv_event_cb_t cb) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_add_event_cb(o, cb, LV_EVENT_DRAW_MAIN, nullptr);
  return o;
}

void line(lv_layer_t* L, float x1, float y1, float x2, float y2, uint32_t color, int w) {
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.color = lv_color_hex(color);
  d.width = w;
  d.round_start = d.round_end = 1;
  d.p1.x = x1; d.p1.y = y1; d.p2.x = x2; d.p2.y = y2;
  lv_draw_line(L, &d);
}

void arc(lv_layer_t* L, int cx, int cy, int r, int a0, int a1, uint32_t color, int w) {
  lv_draw_arc_dsc_t d;
  lv_draw_arc_dsc_init(&d);
  d.color = lv_color_hex(color);
  d.width = w;
  d.center.x = cx; d.center.y = cy;
  d.radius = r;
  d.start_angle = a0; d.end_angle = a1;
  d.rounded = 1;
  lv_draw_arc(L, &d);
}

void dot(lv_layer_t* L, float cx, float cy, int r, uint32_t color) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = lv_color_hex(color);
  d.radius = LV_RADIUS_CIRCLE;
  lv_area_t a = {(int32_t)(cx - r), (int32_t)(cy - r), (int32_t)(cx + r), (int32_t)(cy + r)};
  lv_draw_rect(L, &d, &a);
}

// ---------- icons and the sun path (drawn, no image files) ----------
int wifiState = 0;  // 0 off, 1 weak, 2 connected
void drawWifi(lv_event_t* e) {
  lv_layer_t* L = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(lv_event_get_target_obj(e), &a);
  int cx = a.x1 + 9, cy = a.y1 + 16;
  for (int i = 0; i < 3; i++) arc(L, cx, cy, 4 + i * 5, 225, 315, wifiState == 2 || (wifiState == 1 && i == 0) ? MUTED : TRACK, 2);
  dot(L, cx, cy, 1, wifiState ? MUTED : TRACK);
  if (!wifiState) line(L, a.x1 + 2, a.y1 + 2, a.x1 + 16, a.y1 + 16, MUTED, 2);
}

bool botLooksOk = false;
void drawTelegram(lv_event_t* e) {
  lv_layer_t* L = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(lv_event_get_target_obj(e), &a);
  const float p[][2] = {{22, 3}, {2, 11}, {9, 13.5}, {19, 6}, {11.5, 14.5}, {11.5, 21}, {15, 16.5}, {19.5, 20}, {22, 3}};
  float k = 17 / 24.0f;
  for (int i = 0; i + 1 < 9; i++)
    line(L, a.x1 + p[i][0] * k, a.y1 + p[i][1] * k, a.x1 + p[i + 1][0] * k, a.y1 + p[i + 1][1] * k, botLooksOk ? MUTED : TRACK, 2);
}

void drawMosque(lv_event_t* e) {
  lv_layer_t* L = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(lv_event_get_target_obj(e), &a);
  float k = 40 / 24.0f, x = a.x1, y = a.y1;
  auto ln = [&](float x1, float y1, float x2, float y2) { line(L, x + x1 * k, y + y1 * k, x + x2 * k, y + y2 * k, ON_ACCENT, 3); };
  ln(4, 21, 20, 21);
  ln(6, 21, 6, 13);
  ln(18, 21, 18, 13);
  arc(L, x + 12 * k, y + 13 * k, 6 * k, 180, 360, ON_ACCENT, 3);
  ln(12, 7, 12, 3);
  ln(10.5, 4.5, 12, 3);
  ln(12, 3, 13.5, 4.5);
  ln(10, 21, 10, 17);
  ln(14, 21, 14, 17);
  arc(L, x + 12 * k, y + 17 * k, 2 * k, 180, 360, ON_ACCENT, 3);
}

struct Sun { bool valid; float t; int next; uint8_t past; float tp[6]; } sun;  // t < 0: the sun is below the horizon
void drawSun(lv_event_t* e) {
  if (!sun.valid) return;
  lv_layer_t* L = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(lv_event_get_target_obj(e), &a);
  float ox = a.x1, oy = a.y1;
  line(L, ox, oy + 60, ox + 456, oy + 60, TRACK, 2);
  const int N = 48;
  for (int i = 0; i < N; i++) {
    float t0 = i / (float)N, t1 = (i + 1) / (float)N;
    bool done = sun.t >= 0 && t1 <= sun.t + 0.0001f;
    line(L, ox + sunX(t0), oy + sunY(t0), ox + sunX(t1), oy + sunY(t1), done ? ACCENT : TRACK, 3);
  }
  for (int i = 0; i < 6; i++) {
    float x = i == 0 ? 440 : i == 5 ? 16 : sunX(sun.tp[i]), y = i == 0 || i == 5 ? 60 : sunY(sun.tp[i]);
    bool next = i == sun.next, past = sun.past >> i & 1;
    uint32_t c = next ? (sun.t < 0 ? ACCENT : INK) : past ? DOT_PAST : MUTED;
    dot(L, ox + x, oy + y, next ? 4 : 3, c);
  }
  if (sun.t >= 0) dot(L, ox + sunX(sun.t), oy + sunY(sun.t), 9, ACCENT);
}

// ---------- screens ----------
lv_obj_t *hDate, *hHijri, *hWifi, *hBot, *hAmPm, *hClock, *hNext, *hPlace, *hSun, *hNewsTag, *hNews, *hBanner, *hBannerText;
lv_obj_t *hCard[6], *hCardName[6], *hCardTime[6];
lv_obj_t *nyType, *nyPanel, *nyText, *nySource, *nyFootL;
lv_obj_t *nwNextIn, *nwNextName, *nwRow[3], *nwTitle[3], *nwMeta[3], *nwFootR, *nwFootL;
lv_obj_t *alName, *alTime, *alIcon, *alSent, *alNext, *alBar;
lv_obj_t *stSummary, *stLabel[6], *stValue[6], *stIp, *stFoot;
lv_obj_t *tbCycle, *tbArc, *tbCount, *tbPhrase, *tbTotal, *tbSync;
lv_obj_t *adTitle, *adIndex, *adPanel, *adText, *adCount, *adOf, *adBar;

void title(lv_obj_t* s, const char* t) { setText(label(s, &font_c24, INK, LV_ALIGN_TOP_RIGHT, -12, rowY(&font_c24, 24, 25)), t); }
lv_obj_t* footer(lv_obj_t* s, lv_align_t al, int x) { return label(s, &font_c13, MUTED, al, x, rowY(&font_c13, 13, 302)); }

void buildHome() {
  lv_obj_t* s = scr[HOME] = screen();
  hDate = label(s, &font_c17, INK, LV_ALIGN_TOP_RIGHT, -12, rowY(&font_c17, 17, 20));
  hHijri = label(s, &font_c13, MUTED, LV_ALIGN_TOP_LEFT, 0, 0);  // placed beside the date in renderHome
  hWifi = drawing(s, 12, 11, 18, 18, drawWifi);
  hBot = drawing(s, 40, 11, 17, 17, drawTelegram);
  hAmPm = label(s, &font_c24, MUTED, LV_ALIGN_TOP_LEFT, 12, 112 - (font_c24.line_height - font_c24.base_line));
  hClock = ltr(label(s, &font_clock80, INK, LV_ALIGN_TOP_LEFT, CLOCK_12H ? 40 : 12, 112 - (font_clock80.line_height - font_clock80.base_line)));
  hNext = label(s, &font_c24, INK, LV_ALIGN_TOP_RIGHT, -12, 84 - (font_c24.line_height - font_c24.base_line));
  hPlace = label(s, &font_c13, MUTED, LV_ALIGN_TOP_RIGHT, -12, 104 - (font_c13.line_height - font_c13.base_line));
  hSun = drawing(s, 12, 118, 456, 70, drawSun);
  for (int i = 0; i < 6; i++) {
    hCard[i] = panel(s, 468 - 71 - i * 77, 192, 71, 62, SURFACE);
    hCardName[i] = label(hCard[i], &font_c13, INK2, LV_ALIGN_TOP_MID, 0, rowY(&font_c13, 13, 18));
    setText(hCardName[i], PRAYER_AR[i]);
    hCardTime[i] = ltr(label(hCard[i], &font_c17, INK, LV_ALIGN_TOP_MID, 0, rowY(&font_c17, 17, 42)));
  }
  hNewsTag = label(s, &font_c14, MUTED, LV_ALIGN_TOP_RIGHT, -14, rowY(&font_c14, 14, 280));
  setText(hNewsTag, "أخبار");
  hNews = label(s, &font_c14, INK, LV_ALIGN_TOP_LEFT, 12, rowY(&font_c14, 14, 280));
  lv_obj_set_width(hNews, 408);
  lv_label_set_long_mode(hNews, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_height(hNews, font_c14.line_height);
  hBanner = panel(s, 12, 262, 456, 36, SURFACE);
  hBannerText = label(hBanner, &font_c14, INK, LV_ALIGN_TOP_RIGHT, -12, rowY(&font_c14, 14, 18));
  lv_obj_set_width(hBannerText, 432);
  lv_label_set_long_mode(hBannerText, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
  lv_obj_add_flag(hBanner, LV_OBJ_FLAG_HIDDEN);
}

void buildNoor() {
  lv_obj_t* s = scr[NOOR] = screen();
  title(s, "نور اليوم");
  nyType = label(s, &font_c13, MUTED, LV_ALIGN_TOP_LEFT, 12, rowY(&font_c13, 13, 27));
  nyPanel = panel(s, 12, 48, 456, 238, SURFACE);
  nyText = label(nyPanel, &font_am34, INK, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_width(nyText, 416);
  lv_obj_set_style_text_align(nyText, LV_TEXT_ALIGN_CENTER, 0);
  nySource = label(nyPanel, &font_c14, ACCENT, LV_ALIGN_TOP_MID, 0, 0);
  setText(footer(s, LV_ALIGN_TOP_RIGHT, -12), "منشور اليوم من نور اليقين");
  nyFootL = footer(s, LV_ALIGN_TOP_LEFT, 12);
}

void buildNews() {
  lv_obj_t* s = scr[NEWS] = screen();
  title(s, "الأخبار");
  nwNextIn = ltr(label(s, &font_c13, ACCENT, LV_ALIGN_TOP_LEFT, 12, rowY(&font_c13, 13, 27)));
  nwNextName = label(s, &font_c13, MUTED, LV_ALIGN_TOP_LEFT, 0, 0);  // placed beside the countdown
  for (int i = 0; i < 3; i++) {
    nwRow[i] = panel(s, 12, 48 + i * 82, 456, 76, SURFACE);
    nwTitle[i] = label(nwRow[i], &font_c17, INK, LV_ALIGN_TOP_RIGHT, -14, 4);
    lv_obj_set_size(nwTitle[i], 428, font_c17.line_height * 2);
    lv_label_set_long_mode(nwTitle[i], LV_LABEL_LONG_MODE_DOTS);
    nwMeta[i] = label(nwRow[i], &font_c13, MUTED, LV_ALIGN_TOP_RIGHT, -14, 54);
  }
  nwFootR = footer(s, LV_ALIGN_TOP_RIGHT, -12);
  nwFootL = footer(s, LV_ALIGN_TOP_LEFT, 12);
}

void buildAlert() {
  lv_obj_t* s = scr[ALERT] = screen();
  lv_obj_t* f = panel(s, 18, 14, 444, 222, ACCENT, 16);
  drawing(f, 202, 14, 40, 40, drawMosque);
  setText(label(f, &font_c17, ON_ACCENT, LV_ALIGN_TOP_MID, 0, rowY(&font_c17, 17, 76)), "حان الآن موعد");
  alName = label(f, &font_c52, ON_ACCENT, LV_ALIGN_TOP_MID, 0, rowY(&font_c52, 52, 124));
  alTime = ltr(label(f, &font_c24, ON_ACCENT, LV_ALIGN_TOP_MID, 0, rowY(&font_c24, 24, 186)));
  lv_obj_t* b = panel(s, 18, 246, 444, 44, ALERT_BAR, 12);
  alIcon = label(b, &lv_font_montserrat_16, ACCENT, LV_ALIGN_LEFT_MID, 0, 0);
  alSent = label(b, &font_c13, INK, LV_ALIGN_TOP_RIGHT, -40, rowY(&font_c13, 13, 22));
  lv_obj_align(alIcon, LV_ALIGN_RIGHT_MID, -14, 0);
  alNext = label(b, &font_c13, MUTED, LV_ALIGN_TOP_LEFT, 14, rowY(&font_c13, 13, 22));
  alBar = lv_bar_create(s);
  lv_obj_set_pos(alBar, 18, 300);
  lv_obj_set_size(alBar, 444, 6);
  lv_bar_set_range(alBar, 0, 1000);
  lv_obj_set_style_bg_color(alBar, lv_color_hex(TRACK), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(alBar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(alBar, lv_color_hex(ACCENT), LV_PART_INDICATOR);
}

void buildStatus() {
  lv_obj_t* s = scr[STATUS_SCR] = screen();
  title(s, "حالة الجهاز");
  stSummary = label(s, &font_c13, MUTED, LV_ALIGN_TOP_LEFT, 12, rowY(&font_c13, 13, 27));
  lv_obj_t* p = panel(s, 12, 48, 456, 238, SURFACE);
  const char* labels[6] = {"الواي فاي", "عنوان IP", "مواقيت الصلاة", "الأخبار", "بوت تيليغرام", "مدة التشغيل"};
  for (int i = 0; i < 6; i++) {
    int cy = 20 + i * 39;
    setText(stLabel[i] = label(p, &font_c14, MUTED, LV_ALIGN_TOP_RIGHT, -14, rowY(&font_c14, 14, cy)), labels[i]);
    stValue[i] = label(p, &font_c17, INK, LV_ALIGN_TOP_LEFT, 14, rowY(&font_c17, 17, cy));
  }
  ltr(stValue[1]);
  setText(footer(s, LV_ALIGN_TOP_RIGHT, -12), "اضغط BOOT للرجوع");
  stFoot = ltr(footer(s, LV_ALIGN_TOP_LEFT, 12));
}

void buildTasbeeh() {
  lv_obj_t* s = scr[TASBEEH] = screen();
  title(s, "التسبيح");
  tbCycle = label(s, &font_c13, MUTED, LV_ALIGN_TOP_LEFT, 12, rowY(&font_c13, 13, 27));
  tbArc = lv_arc_create(s);
  lv_obj_set_pos(tbArc, 12, 68);
  lv_obj_set_size(tbArc, 200, 200);
  lv_arc_set_rotation(tbArc, 270);
  lv_arc_set_bg_angles(tbArc, 0, 360);
  lv_arc_set_mode(tbArc, LV_ARC_MODE_REVERSE);  // fills counter-clockwise from the top
  lv_arc_set_range(tbArc, 0, TASBEEH_CYCLE);
  lv_obj_remove_style(tbArc, nullptr, LV_PART_KNOB);
  lv_obj_remove_flag(tbArc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(tbArc, 12, LV_PART_MAIN);
  lv_obj_set_style_arc_width(tbArc, 12, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(tbArc, lv_color_hex(TRACK), LV_PART_MAIN);
  lv_obj_set_style_arc_color(tbArc, lv_color_hex(ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_pad_all(tbArc, 0, 0);
  tbCount = ltr(label(s, &font_count64, INK, LV_ALIGN_TOP_LEFT, 0, 0));
  lv_obj_t* of = label(s, &font_c17, MUTED, LV_ALIGN_TOP_LEFT, 0, 0);
  setText(of, "من " + String(TASBEEH_CYCLE));
  lv_obj_align(of, LV_ALIGN_TOP_LEFT, 112 - 22, rowY(&font_c17, 17, 206));
  tbPhrase = label(s, &font_am40, INK, LV_ALIGN_TOP_RIGHT, -12, 0);
  lv_obj_set_style_text_line_space(tbPhrase, 60 - font_am40.line_height, 0);
  lv_obj_align(tbPhrase, LV_ALIGN_TOP_RIGHT, -12, 108 - (font_am40.line_height - font_am40.base_line));
  lv_obj_t* c = panel(s, 232, 150, 236, 110, SURFACE);
  setText(label(c, &font_c13, MUTED, LV_ALIGN_TOP_RIGHT, -14, rowY(&font_c13, 13, 20)), "مجموع اليوم");
  tbTotal = ltr(label(c, &font_c24, INK, LV_ALIGN_TOP_RIGHT, -14, rowY(&font_c24, 24, 52)));
  tbSync = label(c, &font_c13, MUTED, LV_ALIGN_TOP_RIGHT, -14, rowY(&font_c13, 13, 88));
  setText(footer(s, LV_ALIGN_TOP_RIGHT, -12), "ضغطة +1 · ضغطتان للخروج");
  setText(footer(s, LV_ALIGN_TOP_LEFT, 12), "ضغطة طويلة للتصفير");
}

void buildAdhkar() {
  lv_obj_t* s = scr[ADHKAR] = screen();
  adTitle = label(s, &font_c24, INK, LV_ALIGN_TOP_RIGHT, -12, rowY(&font_c24, 24, 25));
  adIndex = label(s, &font_c13, MUTED, LV_ALIGN_TOP_LEFT, 12, rowY(&font_c13, 13, 27));
  adPanel = panel(s, 12, 48, 456, 238, SURFACE);
  adText = label(adPanel, &font_am40, INK, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_width(adText, 416);
  lv_obj_set_style_text_align(adText, LV_TEXT_ALIGN_CENTER, 0);
  // RTL: the count sits right of centre, "من 100" left of it
  adCount = ltr(label(adPanel, &font_c52, ACCENT, LV_ALIGN_TOP_LEFT, 236, 196 - (font_c52.line_height - font_c52.base_line)));
  adOf = label(adPanel, &font_c17, MUTED, LV_ALIGN_TOP_RIGHT, -236, 196 - (font_c17.line_height - font_c17.base_line));
  adBar = lv_bar_create(adPanel);
  lv_obj_set_pos(adBar, 20, 214);
  lv_obj_set_size(adBar, 416, 6);
  lv_obj_set_style_bg_color(adBar, lv_color_hex(TRACK), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(adBar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(adBar, lv_color_hex(ACCENT), LV_PART_INDICATOR);
  setText(footer(s, LV_ALIGN_TOP_RIGHT, -12), "اضغط BOOT للعدّ");
  setText(footer(s, LV_ALIGN_TOP_LEFT, 12), "من موقع نور اليقين");
}

// ---------- sacred text: largest size that fits, else pages ----------
struct SacredFont { const lv_font_t* f; int px; int maxLines; };
const SacredFont SACRED[3] = {{&font_am40, 40, 2}, {&font_am34, 34, 3}, {&font_am28, 28, 4}};

int sacredLines(const String& shaped, const SacredFont& sf, int width) {
  lv_point_t sz;
  int ls = sf.px * 3 / 2 - sf.f->line_height;  // line pitch 1.5 x size
  lv_text_get_size(&sz, shaped.c_str(), sf.f, 0, ls, width, LV_TEXT_FLAG_NONE);
  return (sz.y + ls) / (sf.f->line_height + ls);
}

// Puts `text` into `l` (inside a box of height boxH), centred in the band [top, top+bandH). Returns lines used.
// pages: filled with the text split into pages when even the smallest size does not fit (else 1 page).
String noorPages[6];
int noorPageCount = 1;

void placeSacred(lv_obj_t* l, const String& shaped, const SacredFont& sf, int top, int bandH, int lines) {
  lv_obj_set_style_text_font(l, sf.f, 0);
  lv_obj_set_style_text_line_space(l, sf.px * 3 / 2 - sf.f->line_height, 0);
  if (strcmp(lv_label_get_text(l), shaped.c_str())) lv_label_set_text(l, shaped.c_str());
  int pitch = sf.px * 3 / 2, block = lines * pitch;
  int firstBaseline = top + (bandH - block) / 2 + pitch * 2 / 3;
  lv_obj_align(l, LV_ALIGN_TOP_MID, 0, firstBaseline - (sf.f->line_height - sf.f->base_line));
}

const SacredFont& fitSacred(const String& shaped, int width, int* lines) {
  for (const auto& sf : SACRED) {
    int n = sacredLines(shaped, sf, width);
    if (n <= sf.maxLines) { *lines = n; return sf; }
  }
  *lines = 4;
  return SACRED[2];
}

void paginate(const char* text, int width) {
  noorPageCount = 0;
  String page, word;
  auto fits = [&](const String& t) { return sacredLines(shapeArabic(t.c_str()), SACRED[2], width) <= 4; };
  String rest = text;
  rest += ' ';
  for (size_t i = 0; i < rest.length(); i++) {
    if (rest[i] != ' ') { word += rest[i]; continue; }
    if (!word.length()) continue;
    String tryPage = page.length() ? page + " " + word : word;
    if (page.length() && !fits(tryPage)) {
      if (noorPageCount < 6) noorPages[noorPageCount++] = page;
      page = word;
    } else page = tryPage;
    word = "";
  }
  if (page.length() && noorPageCount < 6) noorPages[noorPageCount++] = page;
  if (!noorPageCount) noorPages[noorPageCount++] = "";
}

// ---------- state the UI keeps for itself ----------
State s;           // copy of S, refreshed each pass
Noor nr;           // copy of noor, refreshed when its version changes
AdhkarSet ad;      // the set being read
int newsPage = 0, noorPage = 0, newsLineIdx = 0, adSet = 0, adItem = 0, adCountNow = 0;
uint32_t noorVerSeen = 0, alertDismissed = 0, adhkarDoneKey = 0, adhkarKey = 0, lastInput = 0, newsLineAt = 0, adFinishedAt = 0;
bool night = false;
int brightness = -1;

void go(Scr to) {
  if (to == cur) return;
  cur = to;
  lv_screen_load_anim(scr[to], LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}

const Day* today(time_t now) { return findDay(s, ymdPlus(now, 0)); }
const Day* tomorrow(time_t now) { return findDay(s, ymdPlus(now, 1)); }
long secOfDay(time_t now) { tm t; localtime_r(&now, &t); return t.tm_hour * 3600L + t.tm_min * 60 + t.tm_sec; }

String nextLine(time_t now, String* name = nullptr, String* in = nullptr) {
  const Day* d = today(now);
  if (!d || now < 1700000000) return s.wifi ? "جارٍ تحميل المواقيت…" : "بانتظار الاتصال";
  const Day* t = tomorrow(now);
  long ns = secOfDay(now);
  Span sp = currentSpan(d->mins, t ? t->mins : nullptr, ns);
  if (name) *name = String(PRAYER_AR[sp.next]) + " بعد";
  if (in) *in = countdownAr(sp.end - ns);
  return String(PRAYER_AR[sp.next]) + " بعد " + countdownAr(sp.end - ns);
}

// ---------- renderers ----------
void renderHome(time_t now) {
  bool timeOk = now > 1700000000;
  tm t;
  localtime_r(&now, &t);
  int ws = !s.wifi ? 0 : s.rssi < -75 ? 1 : 2;
  if (ws != wifiState) { wifiState = ws; lv_obj_invalidate(hWifi); }
  bool bok = s.botEnabled && s.botOk && s.muteUntil <= now;
  if (bok != botLooksOk) { botLooksOk = bok; lv_obj_invalidate(hBot); }
  lv_obj_set_style_opa(hBot, s.botEnabled ? LV_OPA_COVER : LV_OPA_TRANSP, 0);

  setText(hDate, timeOk ? gregorianAr(t) : String(""));
  setText(hHijri, timeOk ? hijriFor(s, now) : String(""));
  lv_obj_update_layout(hDate);
  lv_obj_align_to(hHijri, hDate, LV_ALIGN_OUT_LEFT_MID, -10, 2);
  setText(hClock, timeOk ? fmtClock(t.tm_hour, t.tm_min) : String("--:--"));
  setText(hAmPm, timeOk && CLOCK_12H ? (t.tm_hour < 12 ? "ص" : "م") : "");
  setText(hNext, nextLine(now));
  setText(hPlace, CITIES[s.city].nameAr);

  const Day* d = timeOk ? today(now) : nullptr;
  if (!d) {
    for (int i = 0; i < 6; i++) {
      setBg(hCard[i], SURFACE);
      setText(hCardTime[i], "--:--");
    }
    if (sun.valid) { sun.valid = false; lv_obj_invalidate(hSun); }
  } else {
    const Day* tm2 = tomorrow(now);
    long ns = secOfDay(now);
    Span sp = currentSpan(d->mins, tm2 ? tm2->mins : nullptr, ns);
    // after Isha the next is tomorrow's Fajr: show its time on the Fajr card
    bool afterIsha = sp.end > 86400;
    for (int i = 0; i < 6; i++) {
      bool next = i == sp.next, past = !next && (afterIsha || d->mins[i] * 60L <= ns);
      setBg(hCard[i], next ? ACCENT : past ? SURFACE_PAST : SURFACE);
      setColor(hCardName[i], next ? ON_ACCENT : past ? PAST : INK2);
      setColor(hCardTime[i], next ? ON_ACCENT : past ? PAST : INK);
      setText(hCardTime[i], fmt24(i == 0 && afterIsha && tm2 ? tm2->mins[0] : d->mins[i]));
    }
    Sun ns2;
    memset(&ns2, 0, sizeof ns2);  // compared with memcmp below, so padding must be zero too
    ns2.valid = true;
    ns2.t = -1;
    ns2.next = sp.next;
    float rise = d->mins[1], set = d->mins[4], nowMin = ns / 60.0f;
    for (int i = 0; i < 6; i++) {
      ns2.tp[i] = constrain((d->mins[i] - rise) / (set - rise), 0.0f, 1.0f);
      if (afterIsha || d->mins[i] * 60L <= ns) ns2.past |= 1 << i;
    }
    ns2.past &= ~(1 << sp.next);
    if (nowMin >= rise && nowMin <= set) ns2.t = (nowMin - rise) / (set - rise);
    ns2.t = ns2.t < 0 ? -1 : floorf(ns2.t * 400) / 400;  // redraw only when the sun visibly moves
    if (memcmp(&ns2, &sun, sizeof sun)) { sun = ns2; lv_obj_invalidate(hSun); }
  }

  // news line: rotates headlines and today's Noor post every NEWS_LINE_MS; a pushed message replaces it
  bool banner = s.banner.text[0] && s.banner.until > now;
  if (banner) {
    lv_obj_remove_flag(hBanner, LV_OBJ_FLAG_HIDDEN);
    setText(hBannerText, s.banner.text);
  } else {
    lv_obj_add_flag(hBanner, LV_OBJ_FLAG_HIDDEN);
    int items = s.nNews + (nr.text[0] ? 1 : 0);
    if (millis() - newsLineAt >= NEWS_LINE_MS) { newsLineAt = millis(); newsLineIdx++; }
    if (!items) setText(hNews, "جارٍ تحميل الأخبار…");
    else {
      int k = newsLineIdx % items;
      bool noorItem = nr.text[0] && k == items - 1;
      setText(hNewsTag, noorItem ? "نور اليوم" : "أخبار");
      setText(hNews, noorItem ? stripMarks(nr.text) : String(s.news[k].title));
      lv_obj_set_width(hNews, noorItem ? 384 : 408);
    }
  }
}

void renderNoor(time_t now) {
  tm t;
  localtime_r(&now, &t);
  const char* kind = !strcmp(nr.type, "hadith") ? "حديث" : !strcmp(nr.type, "dua") ? "دعاء" : "آية";
  setText(nyType, String(kind) + " · " + gregorianAr(t));
  setText(nyFootL, nr.fetched && now - nr.fetched > 4 * 3600 ? "آخر تحديث " + fmt24(localMins(nr.fetched)) : String("يتجدد 8:00 ص"));
  static uint32_t laidOut = 0;
  static int laidPage = -1;
  if (laidOut == noorVerSeen && laidPage == noorPage) return;
  laidOut = noorVerSeen;
  laidPage = noorPage;
  const char* text = nr.text[0] ? nr.text : "لم يصل منشور اليوم بعد";
  String shaped = shapeArabic(text);
  int lines;
  const SacredFont* sf = &fitSacred(shaped, 416, &lines);
  if (sacredLines(shaped, SACRED[2], 416) > 4) {  // too long even at 28 px: page it with BOOT
    paginate(text, 416);
    noorPage %= noorPageCount;
    shaped = shapeArabic(noorPages[noorPage].c_str());
    sf = &SACRED[2];
    lines = min(4, sacredLines(shaped, *sf, 416));
  } else noorPageCount = 1;
  placeSacred(nyText, shaped, *sf, 12, 238 - 12 - 44, lines);
  setText(nySource, String(nr.source) + (noorPageCount > 1 ? "  ·  " + String(noorPage + 1) + "/" + noorPageCount : String("")));
  lv_obj_align(nySource, LV_ALIGN_TOP_MID, 0, rowY(&font_c14, 14, 238 - 30));
}

void renderNews(time_t now) {
  String name, in;
  nextLine(now, &name, &in);
  setText(nwNextIn, in);
  setText(nwNextName, name);
  lv_obj_update_layout(nwNextIn);
  lv_obj_align_to(nwNextName, nwNextIn, LV_ALIGN_OUT_RIGHT_MID, 4, 0);
  int pages = max(1, (s.nNews + 2) / 3);
  newsPage %= pages;
  for (int i = 0; i < 3; i++) {
    int k = newsPage * 3 + i;
    bool has = k < s.nNews;
    if (!s.nNews && i == 0) {
      lv_obj_remove_flag(nwRow[0], LV_OBJ_FLAG_HIDDEN);
      setText(nwTitle[0], "جارٍ تحميل الأخبار…");
      setText(nwMeta[0], "");
      continue;
    }
    if (has) lv_obj_remove_flag(nwRow[i], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(nwRow[i], LV_OBJ_FLAG_HIDDEN);
    if (!has) continue;
    setText(nwTitle[i], s.news[k].title);
    setText(nwMeta[i], String(s.news[k].src) + " · " + ageAr(now, s.news[k].when));
  }
  String upd = s.newsSync ? fmt24(localMins(s.newsSync)) : String("");
  setText(nwFootR, s.newsFailed && s.newsSync ? "تعذّر تحديث الأخبار · آخر نسخة " + upd : s.newsSync ? "آخر تحديث " + upd : String(""));
  setText(nwFootL, String(newsPage + 1) + " من " + pages);
}

void renderAlert(time_t now, const Span& sp) {
  const Day* d = today(now);
  setText(alName, "صلاة " + String(PRAYER_AR[sp.prev]));
  setText(alTime, d ? fmt24(d->mins[sp.prev]) : String(""));
  int8_t sent = s.adhanSent[sp.prev];
  const char* icon = sent > 0 ? LV_SYMBOL_OK : sent < 0 || !s.botEnabled ? LV_SYMBOL_CLOSE : "";
  if (strcmp(lv_label_get_text(alIcon), icon)) lv_label_set_text(alIcon, icon);
  setColor(alIcon, sent > 0 ? ACCENT : MUTED);
  setText(alSent, sent > 0 ? "تم إرسال التذكير على تيليغرام" : sent < 0 || !s.botEnabled ? "لم يُرسل التذكير" : "جارٍ إرسال التذكير…");
  setText(alNext, "التالية: " + String(PRAYER_AR[sp.next]) + " " + fmt24(sp.end / 60));
  long left = ALERT_SECONDS - (secOfDay(now) - sp.start);
  lv_bar_set_value(alBar, (int)constrain(1000L * left / ALERT_SECONDS, 0L, 1000L), LV_ANIM_OFF);
}

void renderStatus(time_t now) {
  const char* problem = !s.wifi ? (s.portal ? "اتصل بشبكة الإعداد" : "لا يوجد اتصال")
                        : s.siteEnabled && s.siteFailed ? "موقع نور اليقين لا يستجيب"
                        : s.newsFailed ? "الأخبار متوقفة"
                        : s.botEnabled && !s.botOk ? "البوت غير متصل"
                        : !today(now) ? "لا توجد مواقيت" : nullptr;
  setText(stSummary, problem ? problem : "كل شيء يعمل");
  setText(stValue[0], s.portal ? String(PORTAL_SSID) + " · وضع الإعداد" : String(s.ssid) + " · " + signalAr(s.rssi));
  setText(stValue[1], s.ip);
  const Day* d = today(now);
  setText(stValue[2], s.prayerSync ? "حُدّثت " + fmt24(localMins(s.prayerSync)) + (d && !d->fromSite ? " · Aladhan" : "")
                                   : String("لم تُحدّث بعد"));
  setText(stValue[3], s.newsSync ? "حُدّثت " + fmt24(localMins(s.newsSync)) : String("لم تُحدّث بعد"));
  setText(stValue[4], !s.botEnabled ? "غير مفعّل" : s.muteUntil > now ? "صامت حتى " + fmt24(localMins(s.muteUntil))
                                                  : s.botOk ? "متصل" : "غير متصل");
  setText(stValue[5], uptimeAr(millis() / 1000));
  setText(stFoot, "fw " + String(FW_VERSION) + " · " + MODULE_MARKING + " · " + (ESP.getFreeHeap() / 1024) + " KB");
}

void renderTasbeeh(time_t now) {
  const Tasbeeh& t = s.tb;
  setText(tbCycle, "الدورة " + String(t.phrase + 1) + " من 3");
  lv_arc_set_value(tbArc, t.count);
  setText(tbCount, String(t.count));
  lv_obj_update_layout(tbCount);
  lv_obj_align(tbCount, LV_ALIGN_TOP_LEFT, 112 - lv_obj_get_width(tbCount) / 2,
               164 - (font_count64.line_height - font_count64.base_line));
  setText(tbPhrase, TASBEEH_AR[t.phrase]);
  setText(tbTotal, String(tasbeehToday(t, ymdPlus(now, 0))));
  bool unsent = t.outDelta || t.pending[0] || t.pending[1] || t.pending[2];
  setText(tbSync, !s.siteEnabled ? "غير مرتبط بالموقع"
                  : unsent && !t.lastSyncOk ? "غير مُزامن · سيُرسل لاحقاً"
                  : t.lastSync ? "مُزامن مع الموقع · " + ageAr(now, t.lastSync) : String("لم يُزامن بعد"));
}

void renderAdhkar(time_t now) {
  setText(adTitle, adSet ? "أذكار المساء" : "أذكار الصباح");
  if (adFinishedAt) {
    setText(adIndex, "");
    placeSacred(adText, shapeArabic("تقبّل الله"), SACRED[0], 0, 238, 1);
    lv_obj_add_flag(adCount, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(adOf, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(adBar, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_remove_flag(adCount, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(adOf, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(adBar, LV_OBJ_FLAG_HIDDEN);
  const Dhikr& d = ad.items[adItem];
  setText(adIndex, "الذكر " + String(adItem + 1) + " من " + ad.n);
  String shaped = shapeArabic(d.text);
  int lines;
  const SacredFont& sf = fitSacred(shaped, 416, &lines);
  placeSacred(adText, shaped, sf, 8, 160, min(lines, 3));
  if (lines > 3) {  // longer than 3 lines even at 28 px: a 3-line window that scrolls slowly
    lv_label_set_long_mode(adText, LV_LABEL_LONG_MODE_SCROLL);
    lv_obj_set_height(adText, 3 * sf.f->line_height + 2 * (sf.px * 3 / 2 - sf.f->line_height));
  } else {
    lv_label_set_long_mode(adText, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_height(adText, LV_SIZE_CONTENT);
  }
  setText(adCount, String(adCountNow));
  setText(adOf, "من " + String(d.count));
  lv_bar_set_range(adBar, 0, d.count);
  lv_bar_set_value(adBar, adCountNow, LV_ANIM_OFF);
}

// ---------- automatic screens, brightness ----------
void autoScreens(time_t now) {
  const Day* d = now > 1700000000 ? today(now) : nullptr;
  if (!d) return;
  long ns = secOfDay(now);
  const Day* t = tomorrow(now);
  Span sp = currentSpan(d->mins, t ? t->mins : nullptr, ns);
  uint32_t ymd = ymdPlus(now, 0);
  uint32_t alertKey = ns - sp.start < ALERT_SECONDS && ns >= sp.start ? ymd * 10 + sp.prev : 0;
  if (alertKey && alertKey != alertDismissed) {
    if (cur != ALERT) { beforeAlert = cur == ADHKAR ? HOME : cur; go(ALERT); }
    renderAlert(now, sp);
    return;
  }
  if (cur == ALERT) go(beforeAlert);

  int nowMin = ns / 60;
  for (int set = 0; set < 2; set++) {
    int start = d->mins[set ? 3 : 0] + ADHKAR_AFTER_MIN;
    uint32_t key = ymd * 2 + set;
    bool inWindow = nowMin >= start && nowMin < start + ADHKAR_FOR_MIN;
    if (inWindow && key != adhkarDoneKey && cur != ADHKAR) {
      Guard g;
      if (!adhkar[set].n) continue;
      ad = adhkar[set];
      adSet = set;
      adItem = adCountNow = 0;
      adFinishedAt = 0;
      adhkarKey = key;
      go(ADHKAR);
    }
    if (cur == ADHKAR && adhkarKey == key && !inWindow) { adhkarDoneKey = key; go(HOME); }
  }
  if (cur == ADHKAR && adFinishedAt && millis() - adFinishedAt >= 3000) { adhkarDoneKey = adhkarKey; adFinishedAt = 0; go(HOME); }
}

void updateBrightness(time_t now) {
  int want = BRIGHT_NORMAL;
  const Day* d = now > 1700000000 ? today(now) : nullptr;
  int m = localMins(now);
  if (cur == ALERT) want = BRIGHT_ALERT;
  else if (night) want = BRIGHT_NIGHT;
  else if (d && (m >= d->mins[5] + DIM_AFTER_ISHA_MIN || m < d->mins[0] - DIM_BEFORE_FAJR_MIN)) want = BRIGHT_DIM;
  if (want != brightness) { brightness = want; lcd.setBrightness(want); }
}

// ---------- input ----------
void onButton(BtnEvent ev) {
  lastInput = millis();
  if (ev == BTN_LONG) {
    if (cur == TASBEEH) { Guard g; S.tb.count = 0; S.tb.phrase = 0; saveTasbeeh(); }
    else night = !night;
    return;
  }
  if (ev == BTN_DOUBLE) {
    if (cur == TASBEEH) go(STATUS_SCR);
    else if (cur == ADHKAR) { adhkarDoneKey = adhkarKey; go(HOME); }
    return;
  }
  switch (cur) {
    case ALERT: {
      time_t now = time(nullptr);
      const Day* d = today(now);
      const Day* t = tomorrow(now);
      if (d) alertDismissed = ymdPlus(now, 0) * 10 + currentSpan(d->mins, t ? t->mins : nullptr, secOfDay(now)).prev;
      go(beforeAlert);
      break;
    }
    case TASBEEH: { Guard g; tasbeehPress(S.tb); saveTasbeeh(); break; }
    case ADHKAR:
      if (adFinishedAt) break;
      if (++adCountNow >= ad.items[adItem].count) {
        adCountNow = 0;
        if (++adItem >= ad.n) adFinishedAt = millis();
        else lv_obj_fade_in(adPanel, 300, 0);
      }
      break;
    case NOOR:
      if (++noorPage >= noorPageCount) { noorPage = 0; go(NEWS); }
      break;
    case NEWS:
      if (++newsPage >= max(1, (s.nNews + 2) / 3)) { newsPage = 0; go(TASBEEH); }
      break;
    case HOME: go(NOOR); break;
    default: go(HOME); break;
  }
}

// ---------- display glue ----------
void flushCb(lv_display_t* d, const lv_area_t* a, uint8_t* px) {
  lcd.pushImage(a->x1, a->y1, lv_area_get_width(a), lv_area_get_height(a), (lgfx::rgb565_t*)px);
  lv_display_flush_ready(d);
}

uint32_t tickCb() { return millis(); }

void uiInit() {
  lcd.init();
  lcd.setRotation(1);  // landscape; use 3 if upside down
  lcd.fillScreen(TFT_BLACK);
  lv_init();
  lv_tick_set_cb(tickCb);
  lv_display_t* disp = lv_display_create(480, 320);
  lv_display_set_flush_cb(disp, flushCb);
  bool ps = psramFound();
  size_t bytes = 480 * (ps ? 40 : 20) * 2;  // DESIGN.md: 2 x 40 lines in PSRAM
  void* b1 = heap_caps_malloc(bytes, ps ? MALLOC_CAP_SPIRAM : MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  void* b2 = ps ? heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM) : nullptr;
  if (!b1) Serial.println("ui: no memory for draw buffer");
  lv_display_set_buffers(disp, b1, b2, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  buildHome();
  buildNoor();
  buildNews();
  buildAlert();
  buildStatus();
  buildTasbeeh();
  buildAdhkar();
  lv_screen_load(scr[HOME]);
}

void render() {
  { Guard g; s = S; if (S.noorVer != noorVerSeen) { nr = noor; noorVerSeen = S.noorVer; } }
  time_t now = time(nullptr);
  autoScreens(now);
  if ((cur == NOOR || cur == NEWS || cur == STATUS_SCR) && millis() - lastInput > SCREEN_TIMEOUT_MS) go(HOME);
  switch (cur) {
    case HOME: renderHome(now); break;
    case NOOR: renderNoor(now); break;
    case NEWS: renderNews(now); break;
    case STATUS_SCR: renderStatus(now); break;
    case TASBEEH: renderTasbeeh(now); break;
    case ADHKAR: renderAdhkar(now); break;
    default: break;
  }
  updateBrightness(now);
}

void uiTask(void*) {
  uiInit();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  esp_task_wdt_add(nullptr);
  Button btn;
  bool level = false;
  uint32_t changed = 0, lastRender = 0;
  for (;;) {
    bool raw = digitalRead(BUTTON_PIN) == LOW;
    if (raw != level && millis() - changed > 30) { changed = millis(); level = raw; }  // debounce
    BtnEvent ev = btn.update(level, millis(), cur == TASBEEH || cur == ADHKAR);
    if (ev != BTN_NONE) { onButton(ev); lastRender = 0; }
    if (!lastRender || millis() - lastRender >= 1000) { lastRender = millis(); render(); }
    lv_timer_handler();
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
