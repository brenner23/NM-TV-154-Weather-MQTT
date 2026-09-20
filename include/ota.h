#ifndef OTA_H
#define OTA_H


/*
 * =========================================================================================
 * WICHTIGER HINWEIS ZUM PARTITION SCHEME (IDE-EINSTELLUNG):
 * =========================================================================================
 * Wenn im Menü "Werkzeuge -> Partition Scheme" folgendes aktiv ist:
 *   [x] "Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)"
 * 
 * ...bricht der OTA-Upload mit "Fehler [1] (OTA_BEGIN_ERROR)" ab, da dieser Sketch
 * mit ca. 1.23 MB zu groß für den 1.2 MB App-Slot ist!
 *
 * LOESUNG:
 * Unter "Werkzeuge -> Partition Scheme" umstellen auf:
 *   -> "Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS)"
 * (Nach dem Ändern zwingend EINMALIG per USB-Kabel flashen, damit die Partitionstabelle
 *  auf den Flash-Speicher des ESP32 geschrieben wird!)
 * =========================================================================================
 */

#include <ArduinoOTA.h>
#include <WiFi.h>
#include <Adafruit_ST7789.h>

extern Adafruit_ST7789 tft;

#define OTA_PASSWORD_PHRASE "dasdaDadaDAsdsadaDASdd"

#define OTA_TFT_BLACK ST77XX_BLACK
#define OTA_TFT_GREEN ST77XX_GREEN
#define OTA_TFT_WHITE ST77XX_WHITE

inline void showOTAScreen() {
  tft.fillScreen(OTA_TFT_BLACK);
  tft.setFont(NULL);
  tft.setTextColor(OTA_TFT_GREEN, OTA_TFT_BLACK);
  tft.setTextSize(3);
  tft.setCursor(80, 70);
  tft.print("OTA");
  tft.setCursor(50, 110);
  tft.print("UPDATE");

  // Äußerer Rahmen für den Ladebalken
  tft.drawRect(28, 160, 184, 18, OTA_TFT_WHITE);
  tft.fillRect(30, 162, 180, 14, OTA_TFT_BLACK);
}

inline void setupOTA() {
  uint8_t mac[6];
  WiFi.macAddress(mac);

  char hostname[32];
  snprintf(hostname, sizeof(hostname), "ESP32-NM-TV-154--%02X-%02X", mac[4], mac[5]);

  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.setPassword(OTA_PASSWORD_PHRASE);
  ArduinoOTA.setPort(3232);

  ArduinoOTA.onStart([]() {
    Serial.println("\n[OTA] Start...");
    showOTAScreen();
  });

  ArduinoOTA.onEnd([]() {
    tft.fillRect(20, 195, 200, 25, OTA_TFT_BLACK);
    tft.setFont(NULL);
    tft.setTextSize(2);
    tft.setTextColor(OTA_TFT_GREEN, OTA_TFT_BLACK);
    tft.setCursor(45, 195);
    tft.print("REBOOT...");
    Serial.println("\n[OTA] Erfolgreich geflasht!");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static int lastReportedPercent = -1;
    int percent = progress / (total / 100);

    // Alle 5% aktualisieren: schont den SPI-Bus und hält den TCP-Puffer frei
    if (percent >= lastReportedPercent + 5 || percent == 100) {
      lastReportedPercent = percent;

      // Grüner Fortschrittsbalken füllen
      int barWidth = (percent * 180) / 100;
      tft.fillRect(30, 162, barWidth, 14, OTA_TFT_GREEN);

      // Prozentanzeige darunter
      tft.fillRect(75, 195, 90, 20, OTA_TFT_BLACK);
      tft.setFont(NULL);
      tft.setTextSize(2);
      tft.setTextColor(OTA_TFT_WHITE, OTA_TFT_BLACK);
      tft.setCursor(85, 195);
      tft.printf("%d%%", percent);

      Serial.printf("[OTA] Fortschritt: %d%%\n", percent);
    }
  });

  ArduinoOTA.onError([](ota_error_t error) {
    tft.fillScreen(OTA_TFT_BLACK);
    tft.setFont(NULL);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_RED, OTA_TFT_BLACK);
    tft.setCursor(25, 90);
    tft.print("OTA FEHLER!");

    tft.setTextSize(1);
    tft.setTextColor(OTA_TFT_WHITE, OTA_TFT_BLACK);
    tft.setCursor(25, 125);

    if (error == OTA_AUTH_ERROR)         tft.print("Auth fehlgeschlagen");
    else if (error == OTA_BEGIN_ERROR)   tft.print("Start fehlgeschlagen (Flash/Slot)");
    else if (error == OTA_CONNECT_ERROR) tft.print("Connect Timeout");
    else if (error == OTA_RECEIVE_ERROR) tft.print("Empfangsfehler");
    else if (error == OTA_END_ERROR)     tft.print("Abschlussfehler");

    Serial.printf("[OTA] Fehler [%u]!\n", error);
    delay(2000);
    ESP.restart();
  });

  ArduinoOTA.begin();
  Serial.printf("[OTA] Bereit unter Hostname: %s\n", hostname);
}

inline void handleOTA() {
  ArduinoOTA.handle();
}

#endif // OTA_H