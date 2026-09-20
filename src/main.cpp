/* =========================================================================
 *  FIX 2026-09-13f  —  Touch entprellt (gegen Selbstausloeser durch Drift)
 *  ---------------------------------------------------------------------
 *  - checkTouchTrigger(): 3x Bestaetigung (je >=20ms) + 1,5s Cooldown.
 *    Einzelne Drift-/Rausch-Ausreisser loesen keinen Moduswechsel mehr aus.
 *  - DEBUG_TOUCH wieder auf 1 (zum Beobachten der Rohwerte). Spaeter 0.
 *  - Font-Includes unveraendert: OCRA10 / OCRA9 / AGENCYB50 (vor mqtt.h).
 *  Frueher: Mode 3, PicoMQTT-Broker, In-Place-Screen, Uhr-Minutentick.
 *  Geaenderte Datei: src/main.cpp
 * ========================================================================= */
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <SPI.h>
#include <time.h>

// ── Grafik ────────────────────────────────────────────────
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// ── Pinout ────────────────────────────────────────────────
#include "pins.h"

// ── Instanzen (müssen VOR config_portal.h stehen) ─────────
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
WebServer       server(80);

#define BL_PWM_FREQ 1000
#define BL_PWM_RES  8

#define DEBUG_TOUCH 0

// Hardware-Dimmer (NM TV: Low-Aktiv über Invertierung)
void applyBrightness(uint8_t percent) {
  percent = constrain(percent, 0, 100);
  uint8_t duty = map(percent, 0, 100, 0, 255);
  ledcWrite(TFT_BACKLIGHT, 255 - duty);
}

// ── Portal, OTA, Forecast & Icons ─────────────────────────
#include "config_portal.h"
#include "ota.h"
#include "icons/weather_icons.h"
#include "icons/tux1_icon.h"
#include "icons.h"
#include "forecast.h"

// ── Fonts ─────────────────────────────────────────────────
#include "fonts/OCRA10pt7b.h"
#include "fonts/OCRA9pt7b.h"
#include "fonts/AGENCYB50pt7b.h"

// ── Mode 3: MQTT ──────────────────────────────────────────
#include "mqtt.h"

// ── Globale Variablen ─────────────────────────────────────
const unsigned long AUTO_FETCH_INTERVAL = 15 * 60 * 1000UL;
const unsigned long WIFI_CHECK_INTERVAL = 60 * 1000UL;

struct WeatherStorage {
  float    temp;
  int      humidity;
  int      pressure;
  int      conditionId;
  char     iconCode[4];
  uint32_t lastFetchEpoch;
};
WeatherStorage currentW = {0.0f, 0, 0, 800, "01d", 0};

int lastMinuteDrawn = -1;
int lastDayDrawn    = -1;
unsigned long lastWiFiCheckTime = 0;

// Touch
bool touchIsPressed = false;

// Ansicht: 0=Live-Wetter, 1=3-Tage-Forecast, 2=MQTT/Tasmota
uint8_t viewMode = VIEW_LIVE;

// ── TOUCH-DEBUG (temporär) ──
inline void debugTouch() {
  static uint32_t lastPrint = 0;
  static int vMin = 999999, vMax = 0;
  int v = touchRead(TOUCH_CAP_PIN);
  if (v < vMin) vMin = v;
  if (v > vMax) vMax = v;
  if (millis() - lastPrint > 150) {
    lastPrint = millis();
    Serial.printf("touch=%5d   min=%5d  max=%5d   -> mitte=%d\n",
                  v, vMin, vMax, (vMin + vMax) / 2);
  }
}

// ── Vektor-Icons für die Live-Ansicht ──────────────────────
void drawThermometerIcon(int x, int y) {
  uint16_t red = ST77XX_RED, white = ST77XX_WHITE;
  tft.fillCircle(x, y + 4, 4, red);
  tft.fillRect(x - 2, y - 6, 4, 8, red);
  tft.fillCircle(x, y - 6, 2, red);
  tft.drawPixel(x - 1, y + 4, white);
}

void drawWaterDropIcon(int x, int y) {
  uint16_t blue = tft.color565(0, 180, 255);
  tft.fillCircle(x, y + 3, 4, blue);
  tft.fillTriangle(x - 4, y + 2, x + 4, y + 2, x, y - 6, blue);
  tft.drawPixel(x - 1, y + 2, ST77XX_WHITE);
}

// ── Live-Wetter NVS ───────────────────────────────────────
void saveWeatherToNVS() {
  Preferences p; p.begin("weather", false);
  p.putFloat("temp",  currentW.temp);
  p.putInt("hum",     currentW.humidity);
  p.putInt("press",   currentW.pressure);
  p.putInt("cond",    currentW.conditionId);
  p.putString("icon", currentW.iconCode);
  p.putULong("epoch", currentW.lastFetchEpoch);
  p.end();
}

void loadWeatherFromNVS() {
  Preferences p; p.begin("weather", true);
  currentW.temp        = p.getFloat("temp", 0.0f);
  currentW.humidity    = p.getInt("hum", 0);
  currentW.pressure    = p.getInt("press", 1013);
  currentW.conditionId = p.getInt("cond", 800);
  String ic = p.getString("icon", "01d");
  strncpy(currentW.iconCode, ic.c_str(), sizeof(currentW.iconCode));
  currentW.lastFetchEpoch = p.getULong("epoch", 0);
  p.end();
}

// ── OpenWeather Abruf mit dynamischer Konfiguration ───────
bool fetchOpenWeather() {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (strlen(cfg.owKey) == 0 || strlen(cfg.owCity) == 0) return false;

  WiFiClient client;
  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/weather?q=" +
               String(cfg.owCity) + "," + String(cfg.owCountry) +
               "&units=" + String(cfg.owUnits) +
               "&lang="  + String(cfg.owLang) +
               "&appid=" + String(cfg.owKey);

  http.begin(client, url);
  int httpCode = http.GET();
  bool success = false;

  if (httpCode == HTTP_CODE_OK) {
    JsonDocument doc;
    if (!deserializeJson(doc, http.getString())) {
      currentW.temp        = doc["main"]["temp"] | 0.0f;
      currentW.humidity    = doc["main"]["humidity"] | 0;
      currentW.pressure    = doc["main"]["pressure"] | 1013;
      currentW.conditionId = doc["weather"][0]["id"] | 800;
      const char* ic       = doc["weather"][0]["icon"] | "01d";
      strncpy(currentW.iconCode, ic, sizeof(currentW.iconCode));
      time_t now; time(&now);
      currentW.lastFetchEpoch = (uint32_t)now;
      saveWeatherToNVS();
      success = true;
    }
  }
  http.end();
  return success;
}

// ── Rendering (Hauptansicht) ──────────────────────────────
void updateDateOnly(struct tm* ptm, bool validTime) {
  tft.fillRect(0, 0, 160, 60, ST77XX_BLACK);
  tft.setFont(&OCRA10pt7b);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_GREEN);

  char dStr[16], wStr[16];
  if (validTime) {
    strftime(dStr, sizeof(dStr), "%d.%m.%Y", ptm);
    strftime(wStr, sizeof(wStr), "%A", ptm);
    for (int i = 0; wStr[i]; i++) wStr[i] = toupper(wStr[i]);
  } else {
    strcpy(dStr, "--.--.----");
    strcpy(wStr, "WARTE NTP");
  }
  tft.setCursor(5, 20); tft.print(dStr);
  tft.setCursor(5, 45); tft.print(wStr);
}

void updateClock(struct tm* ptm, bool validTime) {
  tft.fillRect(0, 72, SCREEN_W, 84, ST77XX_BLACK);
  tft.setFont(&AGENCYB50pt7b);
  tft.setTextSize(1);

  char hStr[4], mStr[4];
  if (validTime) {
    strftime(hStr, sizeof(hStr), "%H", ptm);
    strftime(mStr, sizeof(mStr), "%M", ptm);
  } else {
    strcpy(hStr, "--"); strcpy(mStr, "--");
  }

  int16_t x1, y1;
  uint16_t wHours, hHours, wColon, hColon, wMins, hMins;
  tft.getTextBounds(hStr, 0, 0, &x1, &y1, &wHours, &hHours);
  tft.getTextBounds(":",  0, 0, &x1, &y1, &wColon, &hColon);
  tft.getTextBounds(mStr, 0, 0, &x1, &y1, &wMins, &hMins);

  const int spacing = 4;
  int totalWidth = wHours + spacing + wColon + spacing + wMins;
  int startX = (SCREEN_W - totalWidth) / 2;
  int baseY  = 148;

  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(startX, baseY); tft.print(hStr);

  int colonX = startX + wHours + spacing;
  tft.setCursor(colonX, baseY); tft.print(":");

  int minX = colonX + wColon + spacing;
  tft.setTextColor(tft.color565(229, 169, 60));
  tft.setCursor(minX, baseY); tft.print(mStr);
}

void updateWeatherOnly() {
  tft.fillRect(165, 5, 64, 64, ST77XX_BLACK);
  bool isNight = (strchr(currentW.iconCode, 'n') != nullptr);
  const uint16_t* icon = getIconByOwmId(currentW.conditionId, isNight);
  tft.drawRGBBitmap(165, 5, (uint16_t*)icon, 64, 64);

  tft.fillRect(0, 160, SCREEN_W, 80, ST77XX_BLACK);

  char buf[32];
  uint16_t barBlue = tft.color565(0, 165, 255);

  drawThermometerIcon(12, 172);
  tft.drawRect(24, 168, 96, 9, ST77XX_WHITE);
  tft.fillRect(25, 169, 94, 7, ST77XX_BLACK);
  int tW = constrain(map((int)currentW.temp, -10, 40, 0, 94), 0, 94);
  tft.fillRect(25, 169, tW, 7, barBlue);

  tft.setFont(NULL);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  snprintf(buf, sizeof(buf), "%.0f\xF7" "C", currentW.temp);
  tft.setCursor(130, 165); tft.print(buf);

  drawWaterDropIcon(12, 193);
  tft.drawRect(24, 189, 96, 9, ST77XX_WHITE);
  tft.fillRect(25, 190, 94, 7, ST77XX_BLACK);
  int hW = constrain(map(currentW.humidity, 0, 100, 0, 94), 0, 94);
  tft.fillRect(25, 190, hW, 7, barBlue);

  tft.setFont(&OCRA10pt7b);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  snprintf(buf, sizeof(buf), "%d%%", currentW.humidity);
  tft.setCursor(130, 198); tft.print(buf);

  float cmHg = currentW.pressure * 0.0750062f;
  snprintf(buf, sizeof(buf), "%d hPa | %.1f cmHg", currentW.pressure, cmHg);
  tft.setCursor(10, 226); tft.print(buf);
}

void refreshEntireDisplay() {
  time_t now = time(nullptr);
  struct tm* ptm = localtime(&now);
  bool validTime = (now > 1600000000);
  updateDateOnly(ptm, validTime);
  updateClock(ptm, validTime);
  updateWeatherOnly();
  if (validTime) { lastMinuteDrawn = ptm->tm_min; lastDayDrawn = ptm->tm_mday; }
}

// ── Touch-Entprellung: 3x Bestaetigung (~20ms Abstand) + Cooldown ─────────
// Fix gegen Selbstausloeser durch Drift/Rauschen: es wird erst umgeschaltet,
// wenn der Wert 3 Messungen in Folge (je >=20ms auseinander) unter der
// Schwelle liegt. Nach einem Wechsel 1,5s Sperre. Nicht-blockierend.
bool checkTouchTrigger() {
  static unsigned long lastTriggerTime = 0;   // Start des Cooldowns
  static unsigned long lastSampleTime  = 0;   // Zeit der letzten gewerteten Messung
  static int           belowCount      = 0;   // Treffer unter Schwelle in Folge

  const unsigned long COOLDOWN_MS   = 1500;   // nach Wechsel kein Touch annehmen
  const unsigned long SAMPLE_MS     = 20;     // Mindestabstand zwischen Messungen
  const int           CONFIRM_COUNT = 3;      // so viele Messungen noetig

  int rawVal = touchRead(TOUCH_CAP_PIN);

  // Cooldown nach dem letzten Ausloesen
  if (millis() - lastTriggerTime < COOLDOWN_MS) {
    if (rawVal > (TOUCH_THRESH + 8)) touchIsPressed = false;  // Loslassen weiter erkennen
    return false;
  }

  // Nur alle SAMPLE_MS eine Messung werten (entkoppelt von der Loop-Geschwindigkeit)
  if (millis() - lastSampleTime < SAMPLE_MS) return false;
  lastSampleTime = millis();

  if (rawVal < TOUCH_THRESH) {
    belowCount++;
    if (belowCount >= CONFIRM_COUNT && !touchIsPressed) {
      touchIsPressed  = true;
      lastTriggerTime = millis();
      belowCount      = 0;
      return true;
    }
  } else {
    belowCount = 0;                                 // Ausreisser -> Zaehler zuruecksetzen
    if (rawVal > (TOUCH_THRESH + 8)) touchIsPressed = false;  // sauber losgelassen
  }
  return false;
}

// ── Boot-Log ──────────────────────────────────────────────
int bootLineY = 8;
void printBootLog(const char* tag, const char* msg, uint16_t tagColor) {
  tft.setFont(NULL); tft.setTextSize(1);
  char timeBuf[16];
  snprintf(timeBuf, sizeof(timeBuf), "[%3lu.%03lu000] ", millis() / 1000, millis() % 1000);
  tft.setTextColor(tft.color565(140, 140, 140), ST77XX_BLACK);
  tft.setCursor(4, bootLineY); tft.print(timeBuf);
  tft.setTextColor(tagColor, ST77XX_BLACK); tft.print(tag); tft.print(" ");
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK); tft.print(msg);
  bootLineY += 12;
}

// ── Setup ─────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n>>> NM-TV 154 BEREIT <<<\n");

  pinMode(TFT_EN_PIN, OUTPUT);
  digitalWrite(TFT_EN_PIN, LOW);
  delay(50);

  ledcAttach(TFT_BACKLIGHT, BL_PWM_FREQ, BL_PWM_RES);
  loadDeviceConfig();
  applyBrightness(cfg.brightness);

  SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);
  tft.init(SCREEN_W, SCREEN_H, SPI_MODE3);
  tft.setRotation(2);
  tft.fillScreen(ST77XX_BLACK);

  factoryCounterOnBoot();

  if (!isProvisioned()) {
    Serial.println("[SETUP] Keine Zugangsdaten -> Starte AP-Provisioning");
    cpBeginProvisioningAP();
  }

  tft.drawRGBBitmap(170, 170, (uint16_t*)tux1_icon, 64, 64);
  printBootLog("[ OK ]", "Booting Kernel...", ST77XX_GREEN);
  loadWeatherFromNVS();
  loadForecastFromNVS();
  printBootLog("[ OK ]", "Mounted NVS storage.", ST77XX_GREEN);

  char buf[48];
  snprintf(buf, sizeof(buf), "Connecting: %s", cfg.ssid);
  printBootLog("[INFO]", buf, ST77XX_CYAN);

  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.ssid, cfg.pass);
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 25) { delay(300); retries++; }

  if (WiFi.status() == WL_CONNECTED) {
    snprintf(buf, sizeof(buf), "IP: %s", WiFi.localIP().toString().c_str());
    printBootLog("[ OK ]", buf, ST77XX_GREEN);
    setupOTA();
    printBootLog("[ OK ]", "OTA Service online.", ST77XX_GREEN);
  } else {
    printBootLog("[FAIL]", "WLAN connect failed!", ST77XX_RED);
  }

  printBootLog("[INFO]", "Syncing time...", ST77XX_CYAN);
  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");
  time_t now = 0; int ntpRetries = 0;
  while (now < 1600000000 && ntpRetries < 12) { delay(400); time(&now); ntpRetries++; }
  if (now > 1600000000) {
    struct tm* ptm = localtime(&now);
    strftime(buf, sizeof(buf), "Time: %H:%M:%S OK", ptm);
    printBootLog("[ OK ]", buf, ST77XX_GREEN);
  } else {
    printBootLog("[WARN]", "NTP timeout", ST77XX_YELLOW);
  }

  printBootLog("[INFO]", "Starting Web Server...", ST77XX_CYAN);
  cpAttachRoutes(server);
  server.begin();
  printBootLog("[ OK ]", "Config portal online on :80", ST77XX_GREEN);

  if (WiFi.status() == WL_CONNECTED) {
    if (currentW.lastFetchEpoch == 0 ||
        (now - currentW.lastFetchEpoch > (AUTO_FETCH_INTERVAL / 1000))) {
      printBootLog("[INFO]", "Updating Weather...", ST77XX_CYAN);
      fetchOpenWeather();
      printBootLog("[ OK ]", "Weather sync done.", ST77XX_GREEN);
    }
    updateForecastIfNeeded();
  }

  mqttSetup();   // MQTT-Client vorbereiten (Verbindung passiert in loop())

  delay(1400);
  tft.fillScreen(ST77XX_BLACK);
  refreshEntireDisplay();
}

// ── Loop ──────────────────────────────────────────────────
void loop() {
  handleOTA();

#if DEBUG_TOUCH
  debugTouch();          // <-- FIX 2026-09-12 20:19: Aufruf ergaenzt
#endif

  server.handleClient();
  mqttLoop();
  factoryCounterLoop();

  if (cpSaveRequested) { delay(400); ESP.restart(); }

  time_t now = time(nullptr);
  struct tm* ptm = localtime(&now);
  bool validTime = (now > 1600000000);

  if (millis() - lastWiFiCheckTime >= WIFI_CHECK_INTERVAL) {
    lastWiFiCheckTime = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("\n[WATCHDOG] WLAN getrennt! Reboot...");
      delay(300);
      ESP.restart();
    }
  }

  // Live-Wetter-Timer
  if (validTime && (now - currentW.lastFetchEpoch >= (AUTO_FETCH_INTERVAL / 1000))) {
    if (fetchOpenWeather()) {
      if (viewMode == VIEW_LIVE) updateWeatherOnly();
    }
  }

  // Forecast-Timer
  updateForecastIfNeeded();

  // Touch: Modus weiterschalten  Live -> Forecast -> MQTT -> Live
  if (checkTouchTrigger()) {
    viewMode = (viewMode + 1) % 3;
    switch (viewMode) {
      case VIEW_LIVE:
        tft.fillScreen(ST77XX_BLACK);
        refreshEntireDisplay();
        break;
      case VIEW_FORECAST:
        drawForecastScreen();
        break;
      case VIEW_MQTT:
        tft.fillScreen(ST77XX_BLACK);
        mqttResetClock();
        mqttDrawScreen();
        break;
    }
  }

  // Uhr aktualisieren (nur Live-Ansicht; MQTT-Uhr wird bei neuem Paket gezeichnet)
  if (viewMode == VIEW_LIVE && validTime && ptm->tm_min != lastMinuteDrawn) {
    lastMinuteDrawn = ptm->tm_min;
    updateClock(ptm, true);
    if (ptm->tm_mday != lastDayDrawn) {
      lastDayDrawn = ptm->tm_mday;
      updateDateOnly(ptm, true);
    }
  }

  yield();
}
