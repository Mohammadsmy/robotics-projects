#pragma once
// Streaming RSS reader: fed one byte at a time, keeps only <item><title> and <pubDate>,
// and reports done() after maxItems so the caller can drop the connection early.

struct RssItem { String title; time_t when; };

class RssParser {
 public:
  static const int CAP = 8;
  RssItem items[CAP];
  int n = 0;

  explicit RssParser(int maxItems) : maxItems(maxItems < CAP ? maxItems : CAP) {}
  bool done() const { return n >= maxItems; }

  void feed(char c) {
    switch (st) {
      case TEXT:
        if (c == '<') { st = TAG; tag = ""; }
        else if (field) add(c);
        break;
      case TAG:
        if (c == '>') { onTag(); st = TEXT; break; }
        if (tag.length() < 16) tag += c;
        if (tag == "![CDATA[") { st = CDATA; tail = 0; }
        break;
      case CDATA:
        if (field) add(c);
        tail = tail << 8 | (uint8_t)c;
        if ((tail & 0xFFFFFF) == 0x5D5D3E) {  // "]]>"
          if (field && text.length() >= 3) text.remove(text.length() - 3);
          st = TEXT;
        }
        break;
    }
  }

 private:
  enum { TEXT, TAG, CDATA } st = TEXT;
  String tag, text, title, date;
  int field = 0;  // 1 title, 2 pubDate
  bool inItem = false;
  uint32_t tail = 0;
  int maxItems;

  void add(char c) { if (text.length() < 600) text += c; }

  void onTag() {
    String name = tag;
    int sp = name.indexOf(' ');
    if (sp >= 0) name.remove(sp);
    if (name.endsWith("/")) name.remove(name.length() - 1);
    if (name == "item") { inItem = true; title = date = ""; }
    else if (inItem && (name == "title" || name == "pubDate")) { field = name == "title" ? 1 : 2; text = ""; }
    else if (field && (name == "/title" || name == "/pubDate")) { (field == 1 ? title : date) = text; field = 0; }
    else if (name == "/item" && inItem) {
      inItem = false;
      String t = cleanText(title);
      if (t.length() && n < maxItems) items[n++] = {t, parseRfc822(date.c_str())};
    }
  }
};
