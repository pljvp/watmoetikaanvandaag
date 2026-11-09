#pragma once
// Weerlogica zonder hardware: van API-gegevens naar de getallen op het scherm.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ArduinoJson.h>

enum IconType : uint8_t {
  ICON_SUN, ICON_HALF, ICON_CLOUD, ICON_FOG, ICON_RAIN, ICON_SNOW, ICON_STORM, ICON_MOON,
  ICON_HALFMOON, ICON_STORMRAIN
};

struct DayPart {
  int temp;       // graden Celsius, afgerond
  int mm;         // neerslag in mm, afgerond (som over het dagdeel)
  int prob;       // regenkans in procent (hoogste uur)
  int bft;        // windkracht in Beaufort
  int windDir;    // graden, waar de wind vandaan komt (0 = noord)
  IconType icon;
  int level;      // 1 tot 3: licht, matig, zwaar (alleen regen en sneeuw)
};

#ifndef BIRTHDAYS
#define BIRTHDAYS { {0, 0} }       // {dag, maand}; {0, 0} betekent: geen verjaardag
#endif
inline bool isBirthday(int month, int day) {
  static const uint8_t B[][2] = BIRTHDAYS;
  for (unsigned i = 0; i < sizeof(B) / sizeof(B[0]); i++)
    if (B[i][0] == day && B[i][1] == month) return true;
  return false;
}

// Eén kolom op het scherm: welk dagdeel, het weer en (alleen bij de avond) de maanstand.
struct Column {
  bool birthday;      // op deze dag is er een verjaardag (zie BIRTHDAYS in config.h)
  uint8_t kind;       // 0 = ochtend 06-12, 1 = middag 12-18, 2 = avond 18-24
  int moon;           // 0 tot 7: nieuw ... vol ... laatste kwartier (alleen gebruikt bij de avond)
  DayPart p;
};

struct WeatherModel {
  Column col[3];
};

// Welke drie dagdelen staan er op het scherm? Hangt af van het uur van de verversing.
//   (met de standaard verversingen 11:00, 17:00 en 23:00)
//   voor 11:00 : ochtend, middag, avond (vandaag)
//   11 tot 17  : middag, avond (vandaag), ochtend (morgen)
//   17 tot 23  : avond (vandaag), ochtend, middag (morgen)
//   vanaf 23   : ochtend, middag, avond (morgen)
#ifndef REFRESH_H1
#define REFRESH_H1 11
#define REFRESH_H2 17
#define REFRESH_H3 23
#endif
inline void layoutFor(int hour, uint8_t kind[3], uint8_t dayOff[3]) {
  static const uint8_t K[4][3] = {{0, 1, 2}, {1, 2, 0}, {2, 0, 1}, {0, 1, 2}};
  static const uint8_t D[4][3] = {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {1, 1, 1}};
  int l = (hour < REFRESH_H1) ? 0 : (hour < REFRESH_H2) ? 1 : (hour < REFRESH_H3) ? 2 : 3;
  for (int i = 0; i < 3; i++) { kind[i] = K[l][i]; dayOff[i] = D[l][i]; }
}

// ---------- Hulpfuncties ----------

// Windkracht uit km/u (afronding op hele km/u, tabel van het KNMI).
inline int beaufort(float kmh) {
  static const float upper[12] = {0.5f, 5.5f, 11.5f, 19.5f, 28.5f, 38.5f,
                                  49.5f, 61.5f, 74.5f, 88.5f, 102.5f, 117.5f};
  int b = 0;
  while (b < 12 && kmh >= upper[b]) b++;
  return b;
}

// Maanstand 0..1 (0 = nieuwe maan, 0.5 = volle maan) naar vorm 0..7.
inline int moonIndex(float phase) {
  phase = phase - floorf(phase);
  return ((int)lroundf(phase * 8.0f)) % 8;
}

// Reserve als de API geen maanstand geeft: benadering uit de datum (op een dag nauwkeurig).
inline float moonPhaseFromDate(int y, int m, int d) {
  int a = (14 - m) / 12;
  int yy = y + 4800 - a;
  int mm = m + 12 * a - 3;
  long jdn = d + (153 * mm + 2) / 5 + 365L * yy + yy / 4 - yy / 100 + yy / 400 - 32045;
  double p = fmod(((double)jdn - 2451550.1) / 29.530588853, 1.0);
  if (p < 0) p += 1.0;
  return (float)p;
}

// Soorten weer, oplopend in prioriteit: het zwaarste weer in een dagdeel wint.
enum Cat : uint8_t { CAT_CLEAR, CAT_HALF, CAT_CLOUD, CAT_FOG, CAT_RAIN, CAT_SNOW, CAT_STORM };

inline Cat categoryForCode(int code, bool isDay) {
  Cat c;
  if (code == 0) c = CAT_CLEAR;
  else if (code == 1 || code == 2) c = CAT_HALF;
  else if (code == 3) c = CAT_CLOUD;
  else if (code == 45 || code == 48) c = CAT_FOG;
  else if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) c = CAT_RAIN;
  else if ((code >= 71 && code <= 77) || code == 85 || code == 86) c = CAT_SNOW;
  else if (code >= 95 && code <= 99) c = CAT_STORM;
  else c = CAT_CLOUD;
  if (!isDay && c == CAT_HALF) c = (code == 1) ? CAT_CLEAR : CAT_CLOUD;   // 's nachts geen zon
  return c;
}

// Niveau van regen of sneeuw uit mm per uur (hoogste uur): licht, matig, zwaar.
inline int precipLevel(float maxMmPerHour) {
  if (maxMmPerHour <= 2.5f) return 1;
  if (maxMmPerHour <= 7.5f) return 2;
  return 3;
}

// ---------- Uurwaarden en dagdelen ----------

struct Hourly {
  int n;
  int day[48];                 // 0 = vandaag, 1 = morgen
  int hour[48];
  float temp[48], prob[48], mm[48], wind[48], dir[48];
  int code[48];
  int isDay[48];
  float cloud[48];             // bewolking in %
};

inline void fillPart(DayPart& p, const Hourly& H, int day, int h0, int h1) {
  int cnt = 0, dayHours = 0, nightHours = 0;
  float sumT = 0, schoolT = 0; bool haveSchoolT = false;
  float sumMm = 0, maxProb = 0, sumW = 0, maxMmHour = 0;
  float vx = 0, vy = 0;
  Cat best = CAT_CLEAR;   // zwaarste weer (mist, regen, sneeuw, onweer) in het dagdeel
  float sumCloud = 0;
  bool hasRain = false;   // regen (of onweer met neerslag) in minstens een uur
  for (int i = 0; i < H.n; i++) {
    if (H.day[i] != day || H.hour[i] < h0 || H.hour[i] >= h1) continue;
    cnt++;
    sumT += H.temp[i];
    if (h0 == 6 && H.hour[i] == 8) { schoolT = H.temp[i]; haveSchoolT = true; }   // ochtend: temperatuur om 08:00 (schooltijd)
    sumMm += H.mm[i];
    if (H.mm[i] > maxMmHour) maxMmHour = H.mm[i];
    if (H.prob[i] > maxProb) maxProb = H.prob[i];
    sumW += H.wind[i];
    float rad = H.dir[i] * 0.017453292f;
    vx += H.wind[i] * sinf(rad);
    vy += H.wind[i] * cosf(rad);
    if (H.isDay[i]) dayHours++; else nightHours++;
    sumCloud += H.cloud[i];
    Cat c = categoryForCode(H.code[i], H.isDay[i] != 0);
    if (c == CAT_RAIN || H.mm[i] > 0.0f) hasRain = true;
    if (c >= CAT_FOG && c > best) best = c;
  }
  if (cnt == 0) { memset(&p, 0, sizeof p); p.icon = ICON_CLOUD; return; }
  p.temp = (int)lroundf(haveSchoolT ? schoolT : sumT / cnt);
  p.mm = (int)lroundf(sumMm);
  p.prob = (int)lroundf(maxProb);
  p.bft = beaufort(sumW / cnt);
  float deg = atan2f(vx, vy) * 57.29578f;
  if (deg < 0) deg += 360.0f;
  p.windDir = (int)lroundf(deg) % 360;
  p.level = 0;
  // Zonder zwaar weer bepaalt de gemiddelde bewolking het icoon: onder 20% zon, onder 60% half, anders bewolkt.
  if (best < CAT_FOG) {
    float cc = sumCloud / cnt;
    bool night = nightHours > dayHours;
    if (cc < 20.0f) best = CAT_CLEAR;
    else if (cc < 60.0f) best = CAT_HALF;
    else best = CAT_CLOUD;
  }
  switch (best) {
    case CAT_CLEAR: p.icon = (nightHours > dayHours) ? ICON_MOON : ICON_SUN; break;
    case CAT_HALF:  p.icon = (nightHours > dayHours) ? ICON_HALFMOON : ICON_HALF; break;
    case CAT_CLOUD: p.icon = ICON_CLOUD; break;
    case CAT_FOG:   p.icon = ICON_FOG; break;
    case CAT_RAIN:  p.icon = ICON_RAIN;  p.level = precipLevel(maxMmHour); break;
    case CAT_SNOW:  p.icon = ICON_SNOW;  p.level = precipLevel(maxMmHour); break;
    default:        p.icon = hasRain ? ICON_STORMRAIN : ICON_STORM; break;
  }
}

// Leest het antwoord van Open-Meteo (2 dagen) en vult het model voor een verversing om dit uur.
// Geeft false bij onvolledige gegevens.
inline bool parseForecast(const char* json, WeatherModel& m, int localHour) {
  JsonDocument filter;
  filter["hourly"]["time"] = true;
  filter["hourly"]["temperature_2m"] = true;
  filter["hourly"]["precipitation_probability"] = true;
  filter["hourly"]["precipitation"] = true;
  filter["hourly"]["wind_speed_10m"] = true;
  filter["hourly"]["wind_direction_10m"] = true;
  filter["hourly"]["weather_code"] = true;
  filter["hourly"]["is_day"] = true;
  filter["hourly"]["cloud_cover"] = true;
  filter["daily"]["time"] = true;
  filter["daily"]["moon_phase"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json, DeserializationOption::Filter(filter));
  if (err) return false;

  JsonArrayConst times = doc["hourly"]["time"];
  if (times.isNull() || times.size() < 48) return false;

  static Hourly H;
  H.n = 48;
  for (int i = 0; i < 48; i++) {
    const char* t = times[i] | "";
    if (strlen(t) < 13) return false;
    H.day[i] = i / 24;
    H.hour[i] = atoi(t + 11);
    H.temp[i] = doc["hourly"]["temperature_2m"][i] | 0.0f;
    H.prob[i] = doc["hourly"]["precipitation_probability"][i] | 0.0f;
    H.mm[i]   = doc["hourly"]["precipitation"][i] | 0.0f;
    H.wind[i] = doc["hourly"]["wind_speed_10m"][i] | 0.0f;
    H.dir[i]  = doc["hourly"]["wind_direction_10m"][i] | 0.0f;
    H.code[i] = doc["hourly"]["weather_code"][i] | 3;
    H.isDay[i] = doc["hourly"]["is_day"][i] | 1;
    H.cloud[i] = doc["hourly"]["cloud_cover"][i] | 100.0f;
  }

  uint8_t kind[3], dayOff[3];
  layoutFor(localHour, kind, dayOff);
  static const int H0[3] = {6, 12, 18}, H1[3] = {12, 18, 24};
  for (int i = 0; i < 3; i++) {
    Column& c = m.col[i];
    c.kind = kind[i];
    const char* dt = times[dayOff[i] * 24] | "";                  // datum van deze kolom (jjjj-mm-ddThh:mm)
    c.birthday = strlen(dt) >= 10 && isBirthday(atoi(dt + 5), atoi(dt + 8));
    fillPart(c.p, H, dayOff[i], H0[kind[i]], H1[kind[i]]);
    c.moon = 0;
    if (kind[i] == 2) {                       // maanstand hoort bij de avond in deze kolom
      const char* d = doc["daily"]["time"][dayOff[i]] | "";
      if (strlen(d) < 10) return false;
      JsonVariantConst mp = doc["daily"]["moon_phase"][dayOff[i]];
      float phase = mp.is<float>() ? mp.as<float>() : moonPhaseFromDate(atoi(d), atoi(d + 5), atoi(d + 8));
      c.moon = moonIndex(phase);
    }
  }
  return true;
}
