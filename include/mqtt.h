#ifndef MQTT_H
#define MQTT_H

/* =========================================================================
 *  Mode 3 — MQTT-Empfang von Tasmota  (ESP32 = BROKER via PicoMQTT::Server)
 *  ---------------------------------------------------------------------
 *  FIX 2026-09-13e: Kein Warte-Bildschirm mehr. Sofortige Anzeige des Standard-
 *    Layouts mit 0-Werten. Status-Name initial "Wait...".
 *    Power auf 4-stellig formatiert und bündig zu kWh ausgerichtet.
 * ========================================================================= */

#include <WiFi.h>
#include <PicoMQTT.h>
#include <ArduinoJson.h>
#include <Adafruit_ST7789.h>
#include "config_portal.h"     // cfg

extern Adafruit_ST7789 tft;

// View-Modi (viewMode wird in main.cpp definiert)
#define VIEW_LIVE     0
#define VIEW_FORECAST 1
#define VIEW_MQTT     2
extern uint8_t viewMode;

// Serieller Mitschnitt aller MQTT-Ereignisse (115200). 0 = aus.
#define DEBUG_MQTT 1

// -------------------------------------------------------------
// Datenspeicher fuer das aktuell empfangene Tasmota-Geraet
// -------------------------------------------------------------
struct TasmotaData {
  bool     valid;          // schon Energiedaten empfangen?
  bool     online;         // LWT
  char     name[24];       // aus dem Topic extrahiert
  char     powerState[4];  // "ON"/"OFF"
  float    power;          // [W]
  float    today;          // [kWh]
  float    yesterday;      // [kWh]
  float    total;          // [kWh]
  float    voltage;        // [V]
  float    current;        // [A]
  uint32_t lastMsgMs;
};

// Initialwerte: "Wait..." als Name, "---" als Status, alle Werte auf 0.0
inline TasmotaData tas = { false, false, "Wait...", "---",
                           0, 0, 0, 0, 0, 0, 0 };

inline void mqttDrawScreen();  // Vorwaertsdeklaration

// Anzuzeigender Name: Web-UI-Override > Topic > Default ("Wait...")
inline const char* mqttShownName() {
  if (cfg.mqttName[0])                        return cfg.mqttName;
  if (tas.name[0] && strcmp(tas.name, "---")) return tas.name;
  return "Wait...";
}

// Geraetenamen aus  <prefix>/<device>/<suffix>  ziehen
inline void mqttExtractDevice(const char* topic) {
  const char* p1 = strchr(topic, '/');       if (!p1) return;
  const char* p2 = strchr(p1 + 1, '/');      if (!p2) return;
  size_t n = (size_t)(p2 - (p1 + 1));
  if (n >= sizeof(tas.name)) n = sizeof(tas.name) - 1;
  strncpy(tas.name, p1 + 1, n);
  tas.name[n] = 0;
}

// -------------------------------------------------------------
// Nachrichten-Handler
// -------------------------------------------------------------
inline void mqttOnJson(const char* topic, const char* payload) {
#if DEBUG_MQTT
  Serial.printf("[MQTT<-] %s : %s\n", topic, payload);
#endif
  tas.lastMsgMs = millis();
  mqttExtractDevice(topic);

  JsonDocument doc;
  if (deserializeJson(doc, payload)) return;

  const char* pw = doc["POWER"];
  if (pw) { strncpy(tas.powerState, pw, sizeof(tas.powerState)); tas.powerState[sizeof(tas.powerState)-1]=0; }

  JsonVariantConst energy = doc["ENERGY"];
  if (energy.isNull()) energy = doc["StatusSNS"]["ENERGY"];
  if (!energy.isNull()) {
    tas.power     = energy["Power"]     | tas.power;
    tas.today     = energy["Today"]     | tas.today;
    tas.yesterday = energy["Yesterday"] | tas.yesterday;
    tas.total     = energy["Total"]     | tas.total;
    tas.voltage   = energy["Voltage"]   | tas.voltage;
    tas.current   = energy["Current"]   | tas.current;
    tas.valid     = true;
#if DEBUG_MQTT
    Serial.printf("[MQTT=>] verstanden: Power=%.0fW Today=%.3f Yest=%.3f Total=%.3f  U=%.0fV I=%.3fA  Relay=%s\n",
                  tas.power, tas.today, tas.yesterday, tas.total,
                  tas.voltage, tas.current, tas.powerState);
#endif
  }
  if (viewMode == VIEW_MQTT) mqttDrawScreen();
}

inline void mqttOnPower(const char* topic, const char* payload) {
#if DEBUG_MQTT
  Serial.printf("[MQTT<-] %s : %s\n", topic, payload);
#endif
  tas.lastMsgMs = millis();
  mqttExtractDevice(topic);
  bool on = (strcmp(payload, "ON") == 0 || strcmp(payload, "on") == 0);
  strncpy(tas.powerState, on ? "ON" : "OFF", sizeof(tas.powerState));
  tas.powerState[sizeof(tas.powerState)-1] = 0;
  if (viewMode == VIEW_MQTT) mqttDrawScreen();
}

inline void mqttOnLwt(const char* topic, const char* payload) {
#if DEBUG_MQTT
  Serial.printf("[MQTT<-] %s : %s\n", topic, payload);
#endif
  tas.online = (strcmp(payload, "Online") == 0);
  if (viewMode == VIEW_MQTT) mqttDrawScreen();
}

// -------------------------------------------------------------
// Broker mit User/Passwort-Pruefung
// -------------------------------------------------------------
class NmtvMqttServer : public PicoMQTT::Server {
public:
  NmtvMqttServer(uint16_t port = 1883) : PicoMQTT::Server(port) {}
protected:
  PicoMQTT::ConnectReturnCode auth(const char* client_id,
                                   const char* username,
                                   const char* password) override {
#if DEBUG_MQTT
    Serial.printf("[MQTT] Auth-Versuch client='%s' user='%s'\n",
                  client_id ? client_id : "", username ? username : "");
#endif
    if (strlen(cfg.mqttUser) == 0) return PicoMQTT::CRC_ACCEPTED;  // offen
    if (username && password &&
        strcmp(username, cfg.mqttUser) == 0 &&
        strcmp(password, cfg.mqttPass) == 0) {
      return PicoMQTT::CRC_ACCEPTED;
    }
    Serial.println("[MQTT] Auth abgelehnt (User/Pass falsch)");
    return PicoMQTT::CRC_BAD_USERNAME_OR_PASSWORD;
  }
  void on_connected(const char* client_id) override {
    Serial.printf("[MQTT] Client verbunden: %s\n", client_id ? client_id : "");
  }
  void on_disconnected(const char* client_id) override {
    Serial.printf("[MQTT] Client getrennt: %s\n", client_id ? client_id : "");
  }
};

inline NmtvMqttServer* mqtt = nullptr;

// -------------------------------------------------------------
// Setup (lazy, sobald WLAN steht) + Loop
// -------------------------------------------------------------
inline void mqttSetup() {
  if (mqtt) return;
  if (WiFi.status() != WL_CONNECTED) return;

  uint16_t port = cfg.mqttPort ? cfg.mqttPort : 1883;
  mqtt = new NmtvMqttServer(port);

  mqtt->subscribe("tele/+/SENSOR", mqttOnJson);   // Energie
  mqtt->subscribe("tele/+/STATE",  mqttOnJson);   // enthaelt POWER
  mqtt->subscribe("stat/+/RESULT", mqttOnJson);   // {"POWER":"ON"}
  mqtt->subscribe("stat/+/POWER",  mqttOnPower);  // Klartext ON/OFF
  mqtt->subscribe("tele/+/LWT",    mqttOnLwt);    // Online/Offline
  mqtt->begin();

  Serial.printf("[MQTT] Broker laeuft auf Port %u  —  Tasmota-Host = %s\n",
                port, WiFi.localIP().toString().c_str());
}

inline void mqttLoop() {
  if (!mqtt) { mqttSetup(); return; }
  mqtt->loop();
}

// -------------------------------------------------------------
// Bildschirm (240x240) — In-Place, ohne fillScreen
// -------------------------------------------------------------
inline void mqttDrawRow(int y, const char* label, const char* value) {
  tft.setFont(&OCRA10pt7b);
  tft.setTextSize(1);
  // nur den Wertbereich freiraeumen (Label steht fest)
  tft.fillRect(90, y - 15, 145, 20, ST77XX_BLACK);
  tft.setTextColor(tft.color565(0, 180, 255), ST77XX_BLACK);
  tft.setCursor(12, y);  tft.print(label);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(95, y);  tft.print(value);
}

// Uhr (AGENCYB50) + Datum
inline int mqttClockLastMin = -99;
inline void mqttResetClock() { mqttClockLastMin = -99; }

inline void mqttDrawClock() {
  time_t now = time(nullptr);
  bool valid = (now > 1600000000);
  struct tm* ptm = localtime(&now);
  int curMin = valid ? ptm->tm_min : -1;

  if (curMin == mqttClockLastMin) return;   // gleiche Minute -> nichts tun
  mqttClockLastMin = curMin;

  tft.fillRect(0, 129, SCREEN_W, 111, ST77XX_BLACK);

  char clockStr[8] = "--:--";
  char dateLine[24] = "";
  if (valid) {
    strftime(clockStr, sizeof(clockStr), "%H:%M", ptm);
    char wStr[8], dStr[16];
    strftime(wStr, sizeof(wStr), "%a", ptm);
    strftime(dStr, sizeof(dStr), "%d.%m.%Y", ptm);
    snprintf(dateLine, sizeof(dateLine), "%s %s", wStr, dStr);
  }

  int16_t x1, y1; uint16_t w, hh;

  tft.setFont(&AGENCYB50pt7b);
  tft.setTextSize(1);
  tft.getTextBounds(clockStr, 0, 0, &x1, &y1, &w, &hh);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor((SCREEN_W - w) / 2 - x1, 210);
  tft.print(clockStr);

  if (dateLine[0]) {
    tft.setFont(&OCRA9pt7b);
    tft.getTextBounds(dateLine, 0, 0, &x1, &y1, &w, &hh);
    tft.setTextColor(ST77XX_GREEN);
    tft.setCursor((SCREEN_W - w) / 2 - x1, 230);
    tft.print(dateLine);
  }
}

inline void mqttDrawScreen() {
  tft.setFont(NULL);

  // ── Kopf: Name (Bereich leeren) + ON/OFF-Badge ──
  char nm[20];
  strncpy(nm, mqttShownName(), sizeof(nm));
  nm[sizeof(nm) - 1] = 0;

  tft.fillRect(0, 8, 172, 24, ST77XX_BLACK);
  tft.setFont(&OCRA10pt7b);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  tft.setCursor(10, 24);
  tft.print(nm);

  bool on = (strcmp(tas.powerState, "ON") == 0);
  // Badge: Gruen bei ON, Grau bei OFF oder wenn noch kein Status da ist
  uint16_t badge = on ? tft.color565(0, 200, 60) : tft.color565(70, 70, 70);
  tft.fillRoundRect(178, 6, 54, 22, 4, badge);
  tft.setFont(NULL);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(on ? 190 : 183, 10);
  tft.print(tas.powerState);

  tft.drawFastHLine(10, 34, 220, tft.color565(80, 80, 80));

  // ── Werte formatieren & anzeigen ──
  char v[24];

  // %04.0f W   -> z.B. "0075 W   " (genau 10 Zeichen, 4-stellig mit Nullen)
  // (Falls keine führenden Nullen bei Watt gewünscht sind: "%4.0f W   ")
  snprintf(v, sizeof(v), "%6.0f  W ", tas.power);      mqttDrawRow(50,  "Power", v);
snprintf(v, sizeof(v), "%6.3f kWh", tas.today);      mqttDrawRow(72,  "Today", v);
snprintf(v, sizeof(v), "%6.3f kWh", tas.yesterday);  mqttDrawRow(94,  "Yest.", v);

//snprintf(v, sizeof(v), "%6.1f kWh", tas.total);      mqttDrawRow(116, "Total", v);

if (tas.total >= 10000.0f) {
  snprintf(v, sizeof(v), "%6.0f kWh", tas.total);
} else {
  snprintf(v, sizeof(v), "%6.1f kWh", tas.total);
}
mqttDrawRow(116, "Total", v);


  // Trennstrich zwischen Total und Uhr
  tft.drawFastHLine(10, 128, 220, tft.color565(80, 80, 80));

  // Uhr + Datum drunter
  mqttDrawClock();
}

#endif // MQTT_H