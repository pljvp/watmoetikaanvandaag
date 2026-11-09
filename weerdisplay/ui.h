#pragma once
// Tekenen van het scherm (250 x 122 pixels, zwart-wit). Werkt op elk Adafruit_GFX-object.
// Ontwerp: geen lijnen of balken, veel witruimte, kleine vette koppen en cijfers.
#include <Adafruit_GFX.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "weather.h"

#define INK   0x0000
#define PAPER 0xFFFF

static const char* const DAGDEEL[3] = {"ochtend", "middag", "avond"};

// Indeling in pixels: alleen drie kolommen, zonder lijnen of balken
static const int UI_W = 250, UI_H = 122;
static const int UI_COL_W = 64;                        // breedte van een kolom
static const int UI_COL_X[3] = {14, 93, 172};          // 14 pixels marge, 15 pixels tussenruimte
static const int UI_BASE_HEAD = 20;                    // basislijn van de kolomkoppen
static const int UI_ICON_Y = 31;                       // bovenkant van het icoon
static const int UI_ICON = 32;                         // grootte van het icoon
static const int UI_BASE_TEMP = 52;                    // basislijn van de temperatuur
static const int UI_BASE_RAIN = 89;                    // basislijn van de neerslagregel
static const int UI_WIND_Y = 104;                      // middelpunt van de windregel

// Lettertypen: handgetekende Helvetica-beeldpunten uit de U8g2-collectie
#define FONT_HEAD  u8g2_font_helvB08_tf      // koppen en melding
#define FONT_NUM   u8g2_font_helvB08_tf      // getallen
#define FONT_SMALL u8g2_font_helvR08_tf      // tijden en eenheden
#define FONT_TEMP  u8g2_font_helvB14_tf      // temperatuur

// ---------- Kleine hulpjes ----------

typedef U8G2_FOR_ADAFRUIT_GFX Txt;

static int textW(Txt& u, const uint8_t* font, const char* s) {
  u.setFont(font);
  return u.getUTF8Width(s);
}

static void printAt(Txt& u, const uint8_t* font, int x, int baseline, const char* s) {
  u.setFont(font);
  u.setCursor(x, baseline);
  u.print(s);
}

static void thickLine(Adafruit_GFX& g, int x0, int y0, int x1, int y1) {
  g.drawLine(x0, y0, x1, y1, INK);
  if (abs(x1 - x0) >= abs(y1 - y0)) g.drawLine(x0, y0 + 1, x1, y1 + 1, INK);
  else g.drawLine(x0 + 1, y0, x1 + 1, y1, INK);
}

// ---------- Iconen (witte vulling met zwarte rand) ----------
// Alle maten zijn uitgedrukt in een raster van 28 en worden omgerekend naar de iconmaat u.

struct IconScale {
  float k;
  explicit IconScale(int u) : k(u / 28.0f) {}
  int operator()(float v) const { return (int)lroundf(v * k); }
};

static void cloudShape(Adafruit_GFX& g, const IconScale& S, int x, int y, float dy) {
  int y16 = y + S(16 + dy), y11 = y + S(11 + dy);
  int r1 = S(5), r2 = S(7), r3 = S(5);
  int x1 = x + S(8), x2 = x + S(15), x3 = x + S(21);
  g.fillCircle(x1, y16, r1, INK);
  g.fillCircle(x2, y11, r2, INK);
  g.fillCircle(x3, y16, r3, INK);
  g.fillRect(x1, y16, x3 - x1 + 1, S(6), INK);
  g.fillCircle(x1, y16, r1 - 2, PAPER);
  g.fillCircle(x2, y11, r2 - 2, PAPER);
  g.fillCircle(x3, y16, r3 - 2, PAPER);
  g.fillRect(x1, y16, x3 - x1 + 1, S(6) - 2, PAPER);
}

static void iconSun(Adafruit_GFX& g, const IconScale& S, int cx, int cy, float r, float ray0, float ray1) {
  g.fillCircle(cx, cy, S(r), INK);
  g.fillCircle(cx, cy, S(r) - 2, PAPER);
  for (int k = 0; k < 8; k++) {
    float a = k * 0.78539816f;
    thickLine(g, cx + (int)lroundf(S(ray0) * cosf(a)), cy + (int)lroundf(S(ray0) * sinf(a)),
                 cx + (int)lroundf(S(ray1) * cosf(a)), cy + (int)lroundf(S(ray1) * sinf(a)));
  }
}

static void iconMoon(Adafruit_GFX& g, int u, int x, int y) {
  float k = u / 28.0f, t = 2.0f / k;
  for (int py = 0; py < u; py++) {
    for (int px = 0; px < u; px++) {
      float fx = (px + 0.5f) / k, fy = (py + 0.5f) / k;
      float dA = sqrtf((fx - 13) * (fx - 13) + (fy - 14) * (fy - 14));
      float dB = sqrtf((fx - 20) * (fx - 20) + (fy - 10) * (fy - 10));
      bool outer = dA <= 10.0f && dB > 8.0f;
      bool inner = dA <= 10.0f - t && dB > 8.0f + t;
      if (inner) g.drawPixel(x + px, y + py, PAPER);
      else if (outer) g.drawPixel(x + px, y + py, INK);
    }
  }
}

static void rainStrokes(Adafruit_GFX& g, const IconScale& S, int x, int y, int level) {
  static const int xs1[1] = {14}, xs2[2] = {10, 18}, xs3[3] = {7, 14, 21};
  const int* xs = level == 1 ? xs1 : (level == 2 ? xs2 : xs3);
  for (int i = 0; i < level; i++) thickLine(g, x + S(xs[i] + 1), y + S(20), x + S(xs[i] - 2), y + S(26));
}

static void snowDots(Adafruit_GFX& g, const IconScale& S, int x, int y, int level) {
  static const int xs1[1] = {14}, xs2[2] = {10, 18}, xs3[3] = {7, 14, 21};
  const int* xs = level == 1 ? xs1 : (level == 2 ? xs2 : xs3);
  for (int i = 0; i < level; i++) g.fillCircle(x + S(xs[i]), y + S(23), 1, INK), g.fillCircle(x + S(xs[i]) + 1, y + S(23), 1, INK);
}

static void boltShape(Adafruit_GFX& g, const IconScale& S, int x, int y, int dx) {
  g.fillTriangle(x + S(17 + dx), y + S(17), x + S(9 + dx), y + S(24), x + S(15 + dx), y + S(24), INK);
  g.fillTriangle(x + S(13 + dx), y + S(22), x + S(20 + dx), y + S(22), x + S(11 + dx), y + S(28), INK);
}

// Elke wolk heeft zijn onderrand op y + S(18); regen, sneeuw en bliksem hangen eronder.
static void drawIcon(Adafruit_GFX& g, IconType t, int level, int x, int y, int u) {
  IconScale S(u);
  switch (t) {
    case ICON_SUN:   iconSun(g, S, x + S(14), y + S(11), 6, 9, 13); break;
    case ICON_MOON:  iconMoon(g, S(24), x + S(2), y - S(1)); break;
    case ICON_HALF:  iconSun(g, S, x + S(8), y + S(6), 5, 7, 9); cloudShape(g, S, x + S(3), y, -4); break;
    case ICON_HALFMOON: iconMoon(g, S(16), x, y - S(2)); cloudShape(g, S, x + S(3), y, -4); break;
    case ICON_CLOUD: cloudShape(g, S, x, y, -4); break;
    case ICON_FOG:
      g.fillRect(x + S(3), y + S(1), S(22), 2, INK);  g.fillRect(x + S(7), y + S(7), S(18), 2, INK);
      g.fillRect(x + S(3), y + S(13), S(22), 2, INK); g.fillRect(x + S(7), y + S(19), S(18), 2, INK);
      break;
    case ICON_RAIN:  cloudShape(g, S, x, y, -4); rainStrokes(g, S, x, y, level < 1 ? 1 : level); break;
    case ICON_SNOW:  cloudShape(g, S, x, y, -4); snowDots(g, S, x, y, level < 1 ? 1 : level); break;
    case ICON_STORM: cloudShape(g, S, x, y, -4); boltShape(g, S, x, y, 0); break;
    case ICON_STORMRAIN:
      cloudShape(g, S, x, y, -4); boltShape(g, S, x, y, -5);
      thickLine(g, x + S(21), y + S(20), x + S(18), y + S(26));
      thickLine(g, x + S(27), y + S(20), x + S(24), y + S(26));
      break;
  }
}

// ---------- Maanstand (0 = nieuw ... 4 = vol ... 7 = afnemende sikkel) ----------

static void drawMoonPhase(Adafruit_GFX& g, int cx, int cy, int R, int idx) {
  for (int dy = -R; dy <= R; dy++) {
    for (int dx = -R; dx <= R; dx++) {
      float d = sqrtf((float)(dx * dx + dy * dy));
      if (d > R + 0.4f) continue;
      bool lit;
      float t = 0.5f * sqrtf(fmaxf(0.0f, (float)(R * R - dy * dy)));
      switch (idx) {
        case 0: lit = false; break;
        case 1: lit = dx > t; break;
        case 2: lit = dx > 0; break;
        case 3: lit = dx > -t; break;
        case 4: lit = true; break;
        case 5: lit = dx < t; break;
        case 6: lit = dx < 0; break;
        default: lit = dx < -t; break;
      }
      bool edge = d > R - 1.0f + 0.4f;
      g.drawPixel(cx + dx, cy + dy, (edge || !lit) ? INK : PAPER);
    }
  }
}

// ---------- Kleine symbolen ----------

static const uint16_t CAKE[14] PROGMEM = {      // 15 x 14: taart met drie kaarsjes (vlam van 2 pixels, 1 pixel wit, kaars)
  0x888, 0x888, 0x000, 0x888, 0x888, 0x888, 0x3ffe, 0x2002, 0x3ffe, 0x2002, 0x3ffe, 0x2002, 0x2002, 0x7fff
};
static void drawCake(Adafruit_GFX& g, int x, int y) {
  for (int r = 0; r < 14; r++) {
    uint16_t bits = pgm_read_word(&CAKE[r]);
    for (int c = 0; c < 15; c++) if (bits & (1 << (14 - c))) g.drawPixel(x + c, y + r, INK);
  }
}

static const uint16_t DROP[12] PROGMEM = {      // 12 x 12, omtrek van 2 pixels, punt boven, ronde onderkant
  0x040, 0x0e0, 0x1b0, 0x318, 0x60c, 0xc06, 0xc06, 0xc06, 0xc06, 0x71c, 0x3f8, 0x1f0
};
static void drawDrop(Adafruit_GFX& g, int x, int y) {
  for (int r = 0; r < 12; r++) {
    uint16_t bits = pgm_read_word(&DROP[r]);
    for (int c = 0; c < 12; c++) if (bits & (1 << (11 - c))) g.drawPixel(x + c, y + r, INK);
  }
}

// Krul van drie kwart cirkel (open aan de linkerkant), lijndikte 2
static void windCurl(Adafruit_GFX& g, float cx, float cy, float rc, int x, int y, bool up) {
  for (int py = -3; py <= 24; py++) for (int px = -3; px <= 18; px++) {
    float fx = px + 0.5f - cx, fy = py + 0.5f - cy;
    if (fabsf(sqrtf(fx * fx + fy * fy) - rc) > 0.8f) continue;
    if (up && fx < 0 && fy > 0) continue;
    if (!up && fx < 0 && fy < 0) continue;
    g.drawPixel(x + px, y + py, INK);
  }
}
static void drawWindIcon(Adafruit_GFX& g, int x, int y) {      // 13 x 17: middelste lijn op y + 9
  const float rc = 2.0f;
  int yT = y + 4;
  g.fillRect(x, yT, 8, 2, INK);
  windCurl(g, 8, 4 + 1 - rc, rc, x, y, true);
  g.fillRect(x, yT + 4, 13, 2, INK);
  g.fillRect(x, yT + 8, 5, 2, INK);
  windCurl(g, 5, 4 + 9 + rc - 1, rc, x, y, false);
}

// Pijlen van 12 x 12 pixels voor de 8 windrichtingen; index 0 wijst omhoog (noord), daarna met de klok mee.
static const uint16_t ARROWS[8][12] PROGMEM = {
  {0x020, 0x060, 0x0F0, 0x1F8, 0x3FC, 0x060, 0x060, 0x060, 0x060, 0x060, 0x060, 0x060},
  {0x000, 0x000, 0x0FC, 0x07C, 0x03C, 0x07C, 0x0EC, 0x1C4, 0x380, 0x300, 0x000, 0x000},
  {0x000, 0x000, 0x010, 0x018, 0x01C, 0xFFE, 0xFFF, 0x01C, 0x018, 0x010, 0x000, 0x000},
  {0x000, 0x000, 0x300, 0x380, 0x1C4, 0x0EC, 0x07C, 0x03C, 0x07C, 0x0FC, 0x000, 0x000},
  {0x060, 0x060, 0x060, 0x060, 0x060, 0x060, 0x060, 0x3FC, 0x1F8, 0x0F0, 0x060, 0x020},
  {0x000, 0x000, 0x00C, 0x01C, 0x238, 0x370, 0x3E0, 0x3C0, 0x3E0, 0x3F0, 0x000, 0x000},
  {0x000, 0x000, 0x080, 0x180, 0x380, 0x7FF, 0x7FF, 0x380, 0x180, 0x080, 0x000, 0x000},
  {0x000, 0x000, 0x3F0, 0x3E0, 0x3C0, 0x3E0, 0x370, 0x238, 0x01C, 0x00C, 0x000, 0x000},
};

// Windrichting in Nederlandse letters (waar de wind vandaan komt, zoals in het weerbericht).
static const char* const WINDDIR_NL[8] = {"N", "NO", "O", "ZO", "Z", "ZW", "W", "NW"};
static const char* windDirName(int fromDeg) { return WINDDIR_NL[((int)lroundf(fromDeg / 45.0f)) % 8]; }

// Pijl die wijst waar de wind naartoe waait (de API geeft de richting waar de wind vandaan komt).
static void drawWindArrow(Adafruit_GFX& g, int x, int y, int fromDeg) {
  int idx = ((int)lroundf(((fromDeg + 180) % 360) / 45.0f)) % 8;
  for (int r = 0; r < 12; r++) {
    uint16_t bits = pgm_read_word(&ARROWS[idx][r]);
    for (int c = 0; c < 12; c++)
      if (bits & (1 << (11 - c))) g.drawPixel(x + c, y + r, INK);
  }
}

// Getal in vet met de eenheid klein ernaast. Geeft de x erachter terug.
static int drawNumUnit(Txt& u, int x, int baseline, int value, const char* unit) {
  char b[8]; snprintf(b, sizeof b, "%d", value);
  printAt(u, FONT_NUM, x, baseline, b);
  int nx = x + textW(u, FONT_NUM, b) + 3;
  printAt(u, FONT_SMALL, nx, baseline, unit);
  return nx + textW(u, FONT_SMALL, unit);
}

static void drawTemp(Adafruit_GFX& g, Txt& u, int rightX, int baseline, int temp) {
  char b[8]; snprintf(b, sizeof b, "%d", temp);
  int w = textW(u, FONT_TEMP, b);
  int x = rightX - w - 6;
  printAt(u, FONT_TEMP, x, baseline, b);
  g.drawCircle(x + w + 3, baseline - 10, 2, INK);
}

// ---------- Het hele scherm ----------

static void drawScreen(Adafruit_GFX& g, const WeatherModel& m) {
  g.fillScreen(PAPER);
  Txt u;
  u.begin(g);
  u.setFontMode(1);
  u.setForegroundColor(INK);
  u.setBackgroundColor(PAPER);

  for (int i = 0; i < 3; i++) {
    int x0 = UI_COL_X[i], right = x0 + UI_COL_W;
    const Column& c = m.col[i];
    const DayPart& p = c.p;
    printAt(u, FONT_HEAD, x0, UI_BASE_HEAD, DAGDEEL[c.kind]);
    if (c.birthday) drawCake(g, x0 + textW(u, FONT_HEAD, DAGDEEL[c.kind]) + 5, UI_BASE_HEAD - 14);
    if (c.kind == 2) drawMoonPhase(g, right - 5, UI_BASE_HEAD - 4, 5, c.moon);

    drawIcon(g, p.icon, p.level, x0, UI_ICON_Y, UI_ICON);
    drawTemp(g, u, right, UI_BASE_TEMP, p.temp);

    // Neerslagregel: druppel, mm, kans. Windregel: icoon, windkracht rechts uitgelijnd met mm, pijl links uitgelijnd met "mm".
    drawDrop(g, x0, UI_BASE_RAIN - 12);
    char mmS[8]; snprintf(mmS, sizeof mmS, "%d", p.mm);
    int xNum = x0 + 16;
    printAt(u, FONT_NUM, xNum, UI_BASE_RAIN, mmS);
    int xMm = xNum + textW(u, FONT_NUM, mmS) + 2;
    printAt(u, FONT_SMALL, xMm, UI_BASE_RAIN, "mm");
    char pr[8]; snprintf(pr, sizeof pr, "%d", p.prob);
    int xPr = xMm + textW(u, FONT_SMALL, "mm") + (p.prob >= 100 ? 2 : 4);   // bij 100% even ruim als voor het %-teken
    printAt(u, FONT_NUM, xPr, UI_BASE_RAIN, pr);
    printAt(u, FONT_SMALL, xPr + textW(u, FONT_NUM, pr) + 2, UI_BASE_RAIN, "%");

    drawWindIcon(g, x0, UI_WIND_Y - 9);
    char b[4]; snprintf(b, sizeof b, "%d", p.bft);
    printAt(u, FONT_NUM, xNum + textW(u, FONT_NUM, mmS) - textW(u, FONT_NUM, b), UI_WIND_Y + 4, b);
    drawWindArrow(g, xMm, UI_WIND_Y - 6, p.windDir);
    printAt(u, FONT_NUM, xPr, UI_WIND_Y + 4, windDirName(p.windDir));   // links uitgelijnd met de kans
  }
}

// Infoscherm (knop): tijd, verversingen, batterij en wifi.
static void drawBattery(Adafruit_GFX& g, int x, int yTop, int pct) {
  g.drawRect(x, yTop, 16, 8, INK);
  g.fillRect(x + 16, yTop + 2, 2, 4, INK);
  int w = (12 * pct + 50) / 100;
  if (w > 0) g.fillRect(x + 2, yTop + 2, w, 4, INK);
}
static void drawWifiBars(Adafruit_GFX& g, int x, int yBottom, int bars) {
  for (int i = 0; i < 4; i++) {
    int h = 2 + 2 * i, bx = x + i * 4;
    if (i < bars) g.fillRect(bx, yBottom - h + 1, 3, h, INK);
    else          g.drawRect(bx, yBottom - h + 1, 3, h, INK);
  }
}
struct InfoData { const char* now; const char* last; const char* next; float volt; int pct; bool wifiOk; int rssi; bool lastFailed; };
static void drawInfo(Adafruit_GFX& g, const InfoData& d) {
  g.fillScreen(PAPER);
  Txt u; u.begin(g); u.setFontMode(1); u.setForegroundColor(INK); u.setBackgroundColor(PAPER);
  const int base[5] = {30, 48, 66, 84, 102};
  const int X = 130;
  printAt(u, FONT_SMALL, UI_COL_X[0], base[0], "huidige tijd");          printAt(u, FONT_NUM, X, base[0], d.now);
  printAt(u, FONT_SMALL, UI_COL_X[0], base[1], "laatste verversing");
  char b[24];
  if (d.lastFailed) snprintf(b, sizeof b, "%s mislukt", d.last); else snprintf(b, sizeof b, "%s", d.last);
  printAt(u, FONT_NUM, X, base[1], b);
  printAt(u, FONT_SMALL, UI_COL_X[0], base[2], "volgende verversing");   printAt(u, FONT_NUM, X, base[2], d.next);
  printAt(u, FONT_SMALL, UI_COL_X[0], base[3], "batterij");
  drawBattery(g, X, base[3] - 8, d.pct);
  snprintf(b, sizeof b, "%.1f V  %d%%", d.volt, d.pct);
  for (char* p = b; *p; p++) if (*p == '.') *p = ',';
  printAt(u, FONT_NUM, X + 24, base[3], b);
  printAt(u, FONT_SMALL, UI_COL_X[0], base[4], "wifi");
  if (!d.wifiOk) {
    printAt(u, FONT_NUM, X, base[4], "geen verbinding");
  } else {
    int bars = d.rssi > -55 ? 4 : d.rssi > -65 ? 3 : d.rssi > -75 ? 2 : d.rssi > -85 ? 1 : 0;
    drawWifiBars(g, X, base[4], bars);
    snprintf(b, sizeof b, "%d dBm", d.rssi);
    printAt(u, FONT_NUM, X + 24, base[4], b);
  }
}
