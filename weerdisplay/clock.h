#pragma once
// Verversingsmomenten, slaaptijd met klokcorrectie en het klokdata-logboek.
// Alleen rekenwerk (geen hardware), zodat het ook op een computer getest kan worden.
#include <time.h>
#include <stdint.h>
#include <string.h>

#ifndef REFRESH_H0
#define REFRESH_H0 6
#endif
#ifndef REFRESH_H1
#define REFRESH_H1 11
#define REFRESH_H2 17
#define REFRESH_H3 23
#endif

// Het bord wordt zoveel seconden vóór een verversingsmoment wakker. Wordt het in dit venster
// wakker, dan ververst het meteen: een paar minuten verschil maakt voor het weer niets uit.
#define CLOCK_EARLY_S 300

static const int REFRESH_HOURS[4] = {REFRESH_H0, REFRESH_H1, REFRESH_H2, REFRESH_H3};

// Tijdstip (seconden sinds 1970) van een verversing op dag t met uur h, dagverschuiving dayShift.
inline time_t refreshTime(const struct tm& t, int h, int dayShift) {
  struct tm x = t;
  x.tm_hour = h; x.tm_min = 0; x.tm_sec = 0;
  x.tm_mday += dayShift;
  x.tm_isdst = -1;
  return mktime(&x);
}

// Laatste verversingsmoment op of voor nowT.
inline time_t lastRefreshBefore(const struct tm& t, time_t nowT) {
  for (int i = 3; i >= 0; i--) {
    time_t r = refreshTime(t, REFRESH_HOURS[i], 0);
    if (r <= nowT) return r;
  }
  return refreshTime(t, REFRESH_HOURS[3], -1);          // gisteren de laatste
}

// Eerstvolgende verversingsmoment na nowT.
inline time_t nextRefreshAfter(const struct tm& t, time_t nowT) {
  for (int i = 0; i < 4; i++) {
    time_t r = refreshTime(t, REFRESH_HOURS[i], 0);
    if (r > nowT) return r;
  }
  return refreshTime(t, REFRESH_HOURS[0], 1);           // morgen de eerste
}

// "Effectieve tijd": nu plus het vroege venster. Daarmee bepalen we of er ververst moet worden
// en welke kolommen er horen (zie layoutFor in weather.h).
inline time_t effectiveTime(time_t nowT) { return nowT + CLOCK_EARLY_S; }

inline int32_t refreshSlot(time_t nowT) {
  time_t eff = effectiveTime(nowT);
  struct tm t; localtime_r(&eff, &t);
  return (int32_t)(lastRefreshBefore(t, eff) / 60);
}

inline int effectiveHour(time_t nowT) {
  time_t eff = effectiveTime(nowT);
  struct tm t; localtime_r(&eff, &t);
  return t.tm_hour;
}

// Verversingsmoment dat het infoscherm als "volgende" toont.
inline time_t nextRefreshShown(time_t nowT) {
  time_t eff = effectiveTime(nowT);
  struct tm t; localtime_r(&eff, &t);
  return nextRefreshAfter(t, eff);
}

// Echte seconden tot we wakker moeten worden (verversing min het vroege venster). Kan <= 0 zijn.
inline long secondsUntilWake(time_t nowT) {
  return (long)(nextRefreshShown(nowT) - CLOCK_EARLY_S - nowT);
}

// ---------- Klokcorrectie ----------
// ratio = echte seconden per gevraagde seconde slaap. 1.0 = klok loopt goed.

// Hoeveel slaap vragen we om 'real' echte seconden te slapen?
// Zolang de klok nog niet gemeten is, slapen we eerst 90% en meten dan (de tijd wordt toch gesynchroniseerd).
inline long askedSleep(long real, double ratio, bool calibrated) {
  if (real < 5) real = 5;
  if (!calibrated && real > 1800) return real * 90 / 100;
  long s = (long)(real / ratio + 0.5);
  return s < 5 ? 5 : s;
}

// Werkt de ratio bij na een slaap die direct na een synchronisatie begon. Geeft true als het gelukt is.
inline bool updateRatio(double& ratio, bool& calibrated, time_t sleepStartReal, long asked, time_t tReal) {
  if (asked < 1800 || sleepStartReal < 1700000000 || tReal <= sleepStartReal) return false;
  double r = (double)(tReal - sleepStartReal) / (double)asked;
  if (r < 0.8 || r > 1.25) return false;                // onwaarschijnlijk: negeren
  ratio = calibrated ? 0.5 * ratio + 0.5 * r : r;
  calibrated = true;
  return true;
}

// ---------- Klokdata-logboek (7 dagen) ----------

struct ClockEntry {
  uint32_t epoch;        // echte tijd van de synchronisatie
  int32_t asked;         // gevraagde slaap vooraf in seconden (0 = onbekend)
  int32_t realElapsed;   // echt verstreken sinds het begin van die slaap (0 = onbekend)
  int32_t preError;      // klok van het bord min echte tijd vlak voor de sync, in seconden
};
#define CLOCK_LOG_MAX 64

// Begin van de dag, 6 dagen geleden (lokale tijd): alles daarvoor wordt gewist.
// Op dag 8 verdwijnt zo dag 1; er blijven 7 dagen over (6 volle dagen plus vandaag).
inline time_t logCutoff(time_t nowT) {
  struct tm t; localtime_r(&nowT, &t);
  t.tm_hour = 0; t.tm_min = 0; t.tm_sec = 0; t.tm_mday -= 6; t.tm_isdst = -1;
  return mktime(&t);
}

inline int trimClockLog(ClockEntry* a, int n, time_t cutoff) {
  int k = 0;
  for (int i = 0; i < n; i++) if ((time_t)a[i].epoch >= cutoff) a[k++] = a[i];
  return k;
}

inline int appendClockLog(ClockEntry* a, int n, const ClockEntry& e) {
  if (n >= CLOCK_LOG_MAX) { memmove(a, a + 1, (CLOCK_LOG_MAX - 1) * sizeof(ClockEntry)); n = CLOCK_LOG_MAX - 1; }
  a[n] = e;
  return n + 1;
}
