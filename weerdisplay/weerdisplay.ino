// Weerdisplay voor op de kledingkast: LilyGO TTGO T5 V2.3.1 (ESP32 met 2,13 inch e-ink).
// Ververst vier keer per dag (zie config.h) en slaapt daartussen. Eén knop (GPIO 39) toont een infoscherm. Geen updates via wifi.
#include <Arduino.h>
#include "config.h"
#include "secrets.h"
#include <GxEPD2_BW.h>
#include "weather.h"
#include "ui.h"
#include "net.h"
#include "clock.h"
#include <Preferences.h>

// Aansluitingen van het e-ink-scherm op de T5 V2.3.1 (scherm DEPG0213BN)
#define EPD_CS   5
#define EPD_DC   17
#define EPD_RST  16
#define EPD_BUSY 4

GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(GxEPD2_213_BN(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

// Blijft bewaard tijdens de slaap (niet bij helemaal stroomloos).
RTC_DATA_ATTR int32_t lastSlot = -1;        // laatste verversingsmoment dat is getoond (in minuten sinds 1970)
RTC_DATA_ATTR time_t lastAttemptTime = 0;   // laatste poging tot verversen (gelukt of mislukt)
RTC_DATA_ATTR bool lastFailed = false;      // die poging is mislukt
RTC_DATA_ATTR bool haveModel = false;
RTC_DATA_ATTR WeatherModel savedModel;      // het getoonde weer, om na het infoscherm terug te zetten

static WeatherModel model;

// Klok: hoeveel echte seconden duurt een gevraagde seconde slaap, en de gegevens van de laatste slaap.
RTC_DATA_ATTR float clockRatio = 1.0f;
RTC_DATA_ATTR bool clockCalibrated = false;
RTC_DATA_ATTR time_t sleepStartReal = 0;    // echte tijd bij het begin van de laatste slaap (direct na een sync)
RTC_DATA_ATTR int32_t sleepAskedS = 0;
RTC_DATA_ATTR bool sleepFromSync = false;

static void sleepFor(long seconds) {
  if (seconds < 5) seconds = 5;
  esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ULL);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)INFO_BUTTON_PIN, 0);   // knop (actief laag) wekt het bord
  esp_deep_sleep_start();
}

// Slaap in één keer tot vlak voor het volgende verversingsmoment, met de gemeten klokcorrectie.
// synced = true als de klok net via internet gelijk is gezet (dan kan de volgende slaap de klok meten).
static void sleepUntilNextRefresh(bool synced) {
  time_t nowT = time(nullptr);
  if (nowT < 1700000000) { sleepFromSync = false; sleepFor(15 * 60); }      // klok onbekend: later opnieuw
  long left = secondsUntilWake(nowT);
  long asked = askedSleep(left, clockRatio, clockCalibrated);
  sleepFromSync = synced;
  sleepStartReal = nowT;
  sleepAskedS = (int32_t)asked;
  sleepFor(asked);
}

// ---------- Klokdata-logboek in het flashgeheugen (7 dagen) ----------

static int loadClockLog(Preferences& p, ClockEntry* a) {
  size_t bytes = p.getBytesLength("log");
  if (bytes == 0 || bytes > CLOCK_LOG_MAX * sizeof(ClockEntry)) return 0;
  p.getBytes("log", a, bytes);
  return (int)(bytes / sizeof(ClockEntry));
}

static void logClockSync(const ClockEntry& e) {
  static ClockEntry a[CLOCK_LOG_MAX];
  Preferences p;
  if (!p.begin("klok", false)) return;
  int n = loadClockLog(p, a);
  n = trimClockLog(a, n, logCutoff((time_t)e.epoch));
  n = appendClockLog(a, n, e);
  p.putBytes("log", a, n * sizeof(ClockEntry));
  p.end();
}

// Bij opstarten met USB (reset of stroom aan) zet het logboek in de seriële monitor (115200).
static void printClockLog() {
  static ClockEntry a[CLOCK_LOG_MAX];
  Preferences p;
  if (!p.begin("klok", true)) { Serial.println("klokdata: geen geheugen"); return; }
  int n = loadClockLog(p, a);
  p.end();
  Serial.printf("\nKLOKDATA (laatste 7 dagen, %d regels); ratio nu %.4f%s\n", n, (double)clockRatio, clockCalibrated ? "" : " (nog niet gemeten)");
  Serial.println("datum       tijd      gevraagd  echt      ratio   klok_vooraf");
  for (int i = 0; i < n; i++) {
    time_t t = (time_t)a[i].epoch;
    struct tm x; localtime_r(&t, &x);
    char ratio[12] = "-";
    if (a[i].asked > 0 && a[i].realElapsed > 0) snprintf(ratio, sizeof ratio, "%.4f", (double)a[i].realElapsed / a[i].asked);
    Serial.printf("%04d-%02d-%02d  %02d:%02d:%02d  %-8ld  %-8ld  %-6s  %+ld s\n", x.tm_year + 1900, x.tm_mon + 1, x.tm_mday,
                  x.tm_hour, x.tm_min, x.tm_sec, (long)a[i].asked, (long)a[i].realElapsed, ratio, (long)a[i].preError);
  }
  Serial.flush();
}

static void showModel(const WeatherModel& m) {
  display.init(0);
  display.setRotation(SCREEN_ROTATION);
  display.setFullWindow();
  display.firstPage();
  do {
    drawScreen(display, m);
  } while (display.nextPage());
  display.hibernate();
}

static void formatTime(time_t t, char* out, size_t n) {
  if (t < 1700000000) { snprintf(out, n, "--:--"); return; }   // klok nog niet gezet
  struct tm x;
  localtime_r(&t, &x);
  snprintf(out, n, "%02d:%02d", x.tm_hour, x.tm_min);
}

// Batterijspanning in volt via GPIO 35 (twee gelijke weerstanden: de helft van de spanning).
static float batteryVolts() {
  analogSetPinAttenuation(35, ADC_11db);
  uint32_t mv = 0;
  for (int k = 0; k < 16; k++) { mv += analogReadMilliVolts(35); delay(2); }
  return (mv / 16.0f) * 2.0f / 1000.0f;
}

// Ruwe schatting uit de spanning: 3,3 V is leeg, 4,2 V is vol; afgerond op 10%.
static int batteryPercent(float v) {
  float p = (v - 3.3f) / 0.9f * 100.0f;
  if (p < 0) p = 0;
  if (p > 100) p = 100;
  return ((int)(p + 5.0f) / 10) * 10;
}

// Knop ingedrukt: meet batterij en wifi, toon het infoscherm, wacht op de knop of de tijd,
// zet daarna het weer terug.
static void showInfoThenWeather() {
  setenv("TZ", TZ_INFO, 1);
  tzset();

  float volt = batteryVolts();                           // eerst meten, voordat wifi stroom vraagt
  bool wifiOk = wifiConnect();
  int rssi = wifiOk ? WiFi.RSSI() : 0;
  wifiOff();

  time_t nowT = time(nullptr);
  char nowS[8], lastS[8], nextS[8];
  formatTime(nowT, nowS, sizeof nowS);
  formatTime(lastAttemptTime, lastS, sizeof lastS);
  if (nowT < 1700000000) {
    snprintf(nextS, sizeof nextS, "--:--");
  } else {
    formatTime(nextRefreshShown(nowT), nextS, sizeof nextS);
  }
  InfoData d = {nowS, lastS, nextS, volt, batteryPercent(volt), wifiOk, rssi, lastFailed};

  display.init(0);
  display.setRotation(SCREEN_ROTATION);
  display.setFullWindow();
  display.firstPage();
  do {
    drawInfo(display, d);
  } while (display.nextPage());
  display.hibernate();

  pinMode(INFO_BUTTON_PIN, INPUT);                       // het bord heeft zelf een pull-up
  while (digitalRead(INFO_BUTTON_PIN) == LOW) delay(20); // eerst loslaten
  delay(50);
  unsigned long start = millis();
  while (millis() - start < INFO_SECONDS * 1000UL) {
    if (digitalRead(INFO_BUTTON_PIN) == LOW) break;      // nieuwe druk: terug naar het weer
    delay(20);
  }
  if (haveModel) showModel(savedModel);
}

// Mislukte poging vastleggen en later opnieuw proberen (het oude beeld blijft staan).
static void failAndRetry(long seconds) {
  wifiOff();
  lastAttemptTime = time(nullptr);
  lastFailed = true;
  sleepFromSync = false;
  sleepFor(seconds);
}

void setup() {
  setCpuFrequencyMhz(80);

  if (esp_reset_reason() != ESP_RST_DEEPSLEEP) {          // net aangezet of reset: logboek tonen als USB zit
    Serial.begin(115200);
    delay(1500);
    printClockLog();
  }

  esp_sleep_wakeup_cause_t wake = esp_sleep_get_wakeup_cause();
  if (wake == ESP_SLEEP_WAKEUP_EXT0) {
    showInfoThenWeather();
    sleepUntilNextRefresh(false);
  }

  if (!wifiConnect()) failAndRetry(15 * 60);

  time_t tBefore = time(nullptr);                         // klok van het bord, vlak voor het gelijkzetten
  struct tm now;
  if (!syncTime(now)) failAndRetry(15 * 60);
  time_t tReal = time(nullptr);                           // nu de echte tijd

  // Klok meten en vastleggen
  ClockEntry e = {(uint32_t)tReal, 0, 0, 0};
  if (wake == ESP_SLEEP_WAKEUP_TIMER && sleepFromSync) {
    e.asked = sleepAskedS;
    e.realElapsed = (int32_t)(tReal - sleepStartReal);
    double r = clockRatio; bool c = clockCalibrated;
    if (updateRatio(r, c, sleepStartReal, sleepAskedS, tReal)) { clockRatio = (float)r; clockCalibrated = c; }
  }
  if (tBefore >= 1700000000) e.preError = (int32_t)(tBefore - tReal);
  sleepFromSync = false;
  logClockSync(e);

  int32_t slot = refreshSlot(tReal);
  if (slot != lastSlot) {
    String json;
    bool ok = fetchWeather(json);
    wifiOff();
    if (!ok || !parseForecast(json.c_str(), model, effectiveHour(tReal))) failAndRetry(30 * 60);
    showModel(model);
    savedModel = model;
    haveModel = true;
    lastAttemptTime = tReal;
    lastFailed = false;
    lastSlot = slot;
  } else {
    wifiOff();
  }

  sleepUntilNextRefresh(true);
}

void loop() {}
