#pragma once
// Wifi, tijd en ophalen van het weer. Alleen uitgaande verbindingen; er luistert niets.
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>
#include "config.h"
#include "secrets.h"
#include "certs.h"

// Onthouden tijdens de slaap: kanaal en toegangspunt van de vorige verbinding.
// Met die gegevens hoeft het bord niet eerst alle netwerken te zoeken, en dat scheelt seconden wifi.
RTC_DATA_ATTR bool apKnown = false;
RTC_DATA_ATTR uint8_t apBssid[6];
RTC_DATA_ATTR int32_t apChannel = 0;

static bool waitConnected(int tries) {          // elke 100 ms kijken
  for (int i = 0; i < tries; i++) {
    if (WiFi.status() == WL_CONNECTED) return true;
    delay(100);
  }
  return false;
}

static bool wifiConnect() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  if (apKnown) {                                 // snelle verbinding, maximaal 8 seconden
    WiFi.begin(WIFI_SSID, WIFI_PASS, apChannel, apBssid, true);
    if (waitConnected(80)) return true;
    WiFi.disconnect(false, false);               // lukte niet (router verhuisd?): gewoon zoeken
    apKnown = false;
  }
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  if (!waitConnected(200)) return false;         // maximaal ongeveer 20 seconden
  memcpy(apBssid, WiFi.BSSID(), 6);
  apChannel = WiFi.channel();
  apKnown = true;
  return true;
}

static void wifiOff() {
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
}

// Synchroniseert de klok via NTP. Geeft false als dat niet lukt.
static bool syncTime(struct tm& now) {
  configTzTime(TZ_INFO, "pool.ntp.org", "time.google.com");
  return getLocalTime(&now, 10000);
}

// Haalt het weer van vandaag en morgen op. Geeft false bij een fout; json blijft dan leeg.
static bool fetchWeather(String& json) {
  WiFiClientSecure client;
  client.setCACert(ROOT_CAS);                    // certificaat wordt gecontroleerd
  client.setTimeout(15);
  HTTPClient http;
  String url = String("https://api.open-meteo.com/v1/forecast")
    + "?latitude=" LATITUDE "&longitude=" LONGITUDE
    + "&timezone=" API_TIMEZONE
    + "&forecast_days=2&wind_speed_unit=kmh"
    + "&hourly=temperature_2m,precipitation_probability,precipitation,"
      "wind_speed_10m,wind_direction_10m,weather_code,is_day,cloud_cover"
    + "&daily=moon_phase";
  if (!http.begin(client, url)) return false;
  http.setTimeout(15000);
  int code = http.GET();
  bool ok = (code == HTTP_CODE_OK);
  if (ok) json = http.getString();
  http.end();
  return ok && json.length() > 100;
}
