#ifndef FORECAST_H
#define FORECAST_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Adafruit_ST7789.h>
#include "icons.h"

extern Adafruit_ST7789 tft;

// -------------------------------------------------------------
// 1. Datenstrukturen & globale Variablen (Zuerst deklarieren!)
// -------------------------------------------------------------
struct DayForecast {
  char dayName[4];
  float tempMax;
  float tempMin;
  int conditionId;
  int popPercent;
};

inline DayForecast forecastDays[3];
inline uint32_t lastForecastFetchEpoch = 0;

// Intervall: Nur alle 2 Stunden die Forecast-API befragen
const uint32_t FORECAST_INTERVAL_SEC = 2 * 60 * 60;

// -------------------------------------------------------------
// 2. NVS-Speicherfunktionen für Forecast (mit lokalem 'p')
// -------------------------------------------------------------
inline void saveForecastToNVS() {
  Preferences p;
  p.begin("forecast", false);
  p.putBytes("days", forecastDays, sizeof(forecastDays));
  p.putULong("epoch", lastForecastFetchEpoch);
  p.end();
}

inline void loadForecastFromNVS() {
  Preferences p;
  p.begin("forecast", true);
  if (p.isKey("days")) {
    p.getBytes("days", forecastDays, sizeof(forecastDays));
    lastForecastFetchEpoch = p.getULong("epoch", 0);
  }
  p.end();
}

// -------------------------------------------------------------
// 3. Netzwerk-Abruf mit Cache-Prüfung & RAM-Filter
// -------------------------------------------------------------
inline bool updateForecastIfNeeded(bool forceUpdate = false) {
  time_t now = time(nullptr);

  // Ist der Cache jünger als 2 Stunden?
  if (!forceUpdate && (now - lastForecastFetchEpoch < FORECAST_INTERVAL_SEC)) {
    return true; 
  }

  if (WiFi.status() != WL_CONNECTED) return false;
  if (strlen(cfg.owKey) == 0 || strlen(cfg.owCity) == 0) return false;

  Serial.println("[FORECAST] Hole frische Vorhersagedaten von OpenWeather...");

  WiFiClient client;
  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/forecast?q=" +
               String(cfg.owCity) + "," + String(cfg.owCountry) +
               "&units=" + String(cfg.owUnits) +
               "&lang=" + String(cfg.owLang) +
               "&cnt=32" +
               "&appid=" + String(cfg.owKey);

  http.begin(client, url);
  int httpCode = http.GET();
  bool success = false;

  if (httpCode == HTTP_CODE_OK) {
    JsonDocument filter;
    filter["list"][0]["dt"] = true;
    filter["list"][0]["main"]["temp_max"] = true;
    filter["list"][0]["main"]["temp_min"] = true;
    filter["list"][0]["weather"][0]["id"] = true;
    filter["list"][0]["pop"] = true;
    filter["list"][0]["dt_txt"] = true;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, http.getString(), DeserializationOption::Filter(filter));

    if (!error) {
      struct tm* curTm = localtime(&now);
      int today = curTm->tm_mday;

      int dayCount = 0;
      int lastProcessedDay = today;

      for (int i = 0; i < 3; i++) {
        forecastDays[i].tempMax = -99.0;
        forecastDays[i].tempMin = 99.0;
        forecastDays[i].popPercent = 0;
        forecastDays[i].conditionId = 800;
        strcpy(forecastDays[i].dayName, "---");
      }

      JsonArray list = doc["list"];
      for (JsonObject entry : list) {
        time_t entryTime = entry["dt"];
        struct tm* entryTm = localtime(&entryTime);
        int entryDay = entryTm->tm_mday;

        // Heute überspringen
        if (entryDay == today) continue;

        if (entryDay != lastProcessedDay) {
          if (dayCount < 3 && lastProcessedDay != today) {
            dayCount++;
          }
          if (dayCount >= 3) break;
          lastProcessedDay = entryDay;

          strftime(forecastDays[dayCount].dayName, sizeof(forecastDays[dayCount].dayName), "%a", entryTm);
        }

        float tMax = entry["main"]["temp_max"] | 0.0f;
        float tMin = entry["main"]["temp_min"] | 0.0f;
        if (tMax > forecastDays[dayCount].tempMax) forecastDays[dayCount].tempMax = tMax;
        if (tMin < forecastDays[dayCount].tempMin) forecastDays[dayCount].tempMin = tMin;

        float pop = entry["pop"] | 0.0f;
        int pVal = (int)(pop * 100);
        if (pVal > forecastDays[dayCount].popPercent) forecastDays[dayCount].popPercent = pVal;

        const char* dtTxt = entry["dt_txt"];
        if (strstr(dtTxt, "12:00:00") != nullptr) {
          forecastDays[dayCount].conditionId = entry["weather"][0]["id"] | 800;
        }
      }

      lastForecastFetchEpoch = (uint32_t)now;
      saveForecastToNVS();
      success = true;
    }
  }
  http.end();
  return success;
}

// -------------------------------------------------------------
// 4. Vorhersage-Bildschirm zeichnen (240x240)
// -------------------------------------------------------------
inline void drawForecastScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setFont(NULL);

  tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setCursor(15, 12);
  tft.print("3-TAGE TREND");

  tft.drawFastHLine(10, 34, 220, tft.color565(80, 80, 80));

  for (int i = 0; i < 3; i++) {
    int y = 44 + (i * 64);
    tft.drawRoundRect(8, y, 224, 58, 4, tft.color565(35, 40, 45));

    // Wochentag
    tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setCursor(18, y + 20);
    tft.print(forecastDays[i].dayName);

    // Temperatur Min / Max
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setCursor(65, y + 12);
    tft.printf("%.0f/%.0f\xF7" "C", forecastDays[i].tempMax, forecastDays[i].tempMin);

    // Regenrisiko
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(0, 180, 255), ST77XX_BLACK);
    tft.setCursor(65, y + 36);
    tft.printf("Regen: %d%%", forecastDays[i].popPercent);

    // 32x32 Vektor-Icon
    drawWeatherIconById32(forecastDays[i].conditionId, 180, y + 12);
  }
}

#endif // FORECAST_H