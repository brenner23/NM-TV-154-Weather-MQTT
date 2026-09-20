#ifndef CONFIG_PORTAL_H
#define CONFIG_PORTAL_H

/* =========================================================================
 *  FIX 2026-09-13  —  MQTT als BROKER (Server-Modell)
 *  ---------------------------------------------------------------------
 *  - MQTT-Karte zeigt jetzt die eigene IP (Tasmota-Host). Host/Topic-Felder
 *    entfernt; es bleiben Listen-Port, User, Passwort, Anzeige-Name.
 *  - Struct-Felder mqttHost/mqttTopic bleiben (ungenutzt) fuer NVS-Kompat.
 *  Geaenderte Datei: include/config_portal.h
 * ========================================================================= */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Adafruit_ST7789.h>
#include "credentials.h"

#ifndef HW_RESET_MODE_ENABLED
  #define HW_RESET_MODE_ENABLED 0
#endif
#ifndef HW_RESET_CYCLES
  #define HW_RESET_CYCLES       10
#endif
#ifndef HW_RESET_WINDOW_S
  #define HW_RESET_WINDOW_S     4
#endif

#define CP_AP_PREFIX    "ESP32-WeatherTV-"
#define CP_AP_PASSWORD  "12345678"

extern Adafruit_ST7789 tft;
extern WebServer        server;
extern void applyBrightness(uint8_t percent);

struct DeviceConfig {
  char     ssid[33];
  char     pass[65];
  char     owKey[48];
  char     owCity[40];
  char     owCountry[8];
  char     owUnits[12];
  char     owLang[8];
  uint8_t  brightness;
  // ── MQTT (Mode 3) ──
  char     mqttHost[40];
  uint16_t mqttPort;
  char     mqttUser[32];
  char     mqttPass[33];
  char     mqttTopic[40];   // Tasmota %topic%
  char     mqttName[24];    // Fallback-Anzeigename
};

inline DeviceConfig cfg = {"", "", "", "", "DE", "metric", "de", 80,
                           "", 1883, "", "", "", ""};
inline volatile bool cpSaveRequested = false;

inline void cpCopy(char* dst, const char* src, size_t n) {
  strncpy(dst, src, n - 1);
  dst[n - 1] = '\0';
}

inline String cpBuildApName() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[24];
  snprintf(buf, sizeof(buf), "%s%02X%02X", CP_AP_PREFIX, mac[4], mac[5]);
  return String(buf);
}

inline void loadDeviceConfig() {
  Preferences p;
  p.begin("devcfg", true);
  cpCopy(cfg.ssid,      p.getString("ssid",    "").c_str(),      sizeof(cfg.ssid));
  cpCopy(cfg.pass,      p.getString("pass",    "").c_str(),      sizeof(cfg.pass));
  cpCopy(cfg.owKey,     p.getString("owkey",   "").c_str(),      sizeof(cfg.owKey));
  cpCopy(cfg.owCity,    p.getString("owcity",  "").c_str(),      sizeof(cfg.owCity));
  cpCopy(cfg.owCountry, p.getString("owctry",  "DE").c_str(),    sizeof(cfg.owCountry));
  cpCopy(cfg.owUnits,   p.getString("owunits", "metric").c_str(),sizeof(cfg.owUnits));
  cpCopy(cfg.owLang,    p.getString("owlang",  "de").c_str(),    sizeof(cfg.owLang));
  cfg.brightness = p.getUChar("bright", 80);
  // MQTT
  cpCopy(cfg.mqttHost,  p.getString("mqhost",  "").c_str(),      sizeof(cfg.mqttHost));
  cfg.mqttPort = p.getUShort("mqport", 1883);
  cpCopy(cfg.mqttUser,  p.getString("mquser",  "").c_str(),      sizeof(cfg.mqttUser));
  cpCopy(cfg.mqttPass,  p.getString("mqpass",  "").c_str(),      sizeof(cfg.mqttPass));
  cpCopy(cfg.mqttTopic, p.getString("mqtopic", "").c_str(),      sizeof(cfg.mqttTopic));
  cpCopy(cfg.mqttName,  p.getString("mqname",  "").c_str(),      sizeof(cfg.mqttName));
  p.end();

#if USE_FALLBACK_CREDENTIALS
  if (strlen(cfg.ssid) == 0) {
    cpCopy(cfg.ssid,      FALLBACK_WIFI_SSID,  sizeof(cfg.ssid));
    cpCopy(cfg.pass,      FALLBACK_WIFI_PASS,  sizeof(cfg.pass));
    cpCopy(cfg.owKey,     FALLBACK_OW_KEY,     sizeof(cfg.owKey));
    cpCopy(cfg.owCity,    FALLBACK_OW_CITY,    sizeof(cfg.owCity));
    cpCopy(cfg.owCountry, FALLBACK_OW_COUNTRY, sizeof(cfg.owCountry));
    cpCopy(cfg.owUnits,   FALLBACK_OW_UNITS,   sizeof(cfg.owUnits));
    cpCopy(cfg.owLang,    FALLBACK_OW_LANG,    sizeof(cfg.owLang));
  }
#endif
}

inline void saveDeviceConfig() {
  Preferences p;
  p.begin("devcfg", false);
  p.putString("ssid",   cfg.ssid);
  p.putString("pass",   cfg.pass);
  p.putString("owkey",  cfg.owKey);
  p.putString("owcity", cfg.owCity);
  p.putString("owctry", cfg.owCountry);
  p.putString("owunits",cfg.owUnits);
  p.putString("owlang", cfg.owLang);
  p.putUChar("bright",  cfg.brightness);
  // MQTT
  p.putString("mqhost",  cfg.mqttHost);
  p.putUShort("mqport",  cfg.mqttPort);
  p.putString("mquser",  cfg.mqttUser);
  p.putString("mqpass",  cfg.mqttPass);
  p.putString("mqtopic", cfg.mqttTopic);
  p.putString("mqname",  cfg.mqttName);
  p.end();
}

inline bool isProvisioned() { return strlen(cfg.ssid) > 0; }

inline void factoryReset() {
  Preferences p;
  p.begin("devcfg", false);   p.clear(); p.end();
  p.begin("weather", false);  p.clear(); p.end();
  p.begin("forecast", false); p.clear(); p.end();
  p.begin("boot", false);     p.clear(); p.end();
  delay(200);
  ESP.restart();
}

inline void factoryCounterOnBoot() {
#if HW_RESET_MODE_ENABLED
  Preferences p;
  p.begin("boot", false);
  uint8_t cnt = p.getUChar("cnt", 0) + 1;
  p.putUChar("cnt", cnt);
  p.end();

  if (cnt >= HW_RESET_CYCLES) {
    tft.fillScreen(ST77XX_BLACK);
    tft.setFont(NULL);
    tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
    tft.setTextSize(3);
    tft.setCursor(15, 100); tft.print("FACTORY");
    tft.setCursor(30, 135); tft.print("RESET");
    delay(1500);
    factoryReset();
  } else if (cnt > 1) {
    tft.setFont(NULL);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setTextSize(1);
    char b[24];
    snprintf(b, sizeof(b), "Reset %d/%d", cnt, HW_RESET_CYCLES);
    tft.fillRect(0, 225, 120, 12, ST77XX_BLACK);
    tft.setCursor(4, 227); tft.print(b);
  }
#endif
}

inline void factoryCounterLoop() {
#if HW_RESET_MODE_ENABLED
  static bool cleared = false;
  if (!cleared && millis() > (uint32_t)HW_RESET_WINDOW_S * 1000UL) {
    cleared = true;
    Preferences p;
    p.begin("boot", false);
    p.putUChar("cnt", 0);
    p.end();
  }
#endif
}

inline void cpShowSetupScreen(const String& ap, const String& ip) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setFont(NULL);

  tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  tft.setTextSize(3);
  tft.setCursor(10, 20); tft.print("SETUP");
  tft.setCursor(10, 50); tft.print("MODE");

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(10, 100); tft.print("WLAN:");
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  tft.setCursor(60, 100); tft.print(ap);

  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(10, 120); tft.print("PASS:");
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  tft.setCursor(60, 120); tft.print(CP_AP_PASSWORD);

  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(10, 140); tft.print("IP:");
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  tft.setCursor(60, 140); tft.print(ip);

  tft.setTextColor(tft.color565(150, 150, 150), ST77XX_BLACK);
  tft.setCursor(10, 180); tft.print("-> Handy mit WLAN");
  tft.setCursor(10, 195); tft.print("   verbinden, dann");
  tft.setCursor(10, 210); tft.print("   http://" + ip);
}

inline String cpConfigPageHTML() {
  String h = F("<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>NM-TV Setup</title><style>"
    "body{background:#0e1113;color:#eee;font-family:sans-serif;margin:0;padding:18px;}"
    ".card{background:#181b1e;border-radius:12px;padding:18px;max-width:420px;margin:0 auto 14px;box-shadow:0 4px 15px rgba(0,0,0,.5);}"
    "h3{color:#00e5ff;letter-spacing:1px;font-size:13px;margin:0 0 12px;}"
    "label{display:block;font-size:12px;color:#8a9096;margin:10px 0 4px;}"
    "input[type=text],input[type=password]{width:100%;box-sizing:border-box;background:#0e1113;border:1px solid #2d3339;color:#eee;padding:9px;border-radius:6px;font-size:14px;}"
    "input[type=range]{width:100%;accent-color:#00ff55;}"
    ".net{padding:7px 9px;border:1px solid #2d3339;border-radius:6px;margin:4px 0;cursor:pointer;font-size:13px;display:flex;justify-content:space-between;}"
    ".net:hover{background:#22262a;}"
    ".btn{background:#00ff55;color:#000;border:none;width:100%;padding:13px;font-weight:bold;letter-spacing:1px;border-radius:8px;cursor:pointer;margin-top:14px;}"
    ".btn:hover{background:#00cc44;}"
    ".btnr{background:#ff4444;color:#fff;}"
    ".btnr:hover{background:#cc0000;}"
    ".msg{color:#00ff55;font-size:12px;text-align:center;height:16px;margin-top:8px;font-family:monospace;}"
    "small{color:#8a9096;font-size:11px;}"
    "</style></head><body>");

  // WLAN
  h += F("<div class='card'><h3>WLAN</h3>"
    "<button class='btn' style='background:#2d3339;color:#eee' onclick='scan()'>Netze scannen</button>"
    "<div id='nets'></div>"
    "<label>SSID</label><input type='text' id='ssid' value='");
  h += cfg.ssid;
  h += F("'><label>Passwort <small>(leer = unverändert)</small></label>"
    "<input type='password' id='pass' placeholder='********'></div>");

  // OpenWeather
  h += F("<div class='card'><h3>OPENWEATHER</h3>"
    "<label>API Key</label><input type='text' id='owkey' value='");
  h += cfg.owKey;
  h += F("'><label>Ort</label><input type='text' id='owcity' value='");
  h += cfg.owCity;
  h += F("'><label>Land</label><input type='text' id='owctry' value='");
  h += cfg.owCountry;
  h += F("'></div>");

  // MQTT / Tasmota  (dieses Geraet = Broker)
  h += F("<div class='card'><h3>MQTT / TASMOTA</h3>"
    "<small>Dieses Display ist der MQTT-Broker. Im Tasmota unter "
    "<b>MQTT &rarr; Host</b> diese Adresse eintragen:</small>"
    "<div style='color:#00ff55;font-family:monospace;font-size:15px;margin:8px 0'>");
  h += WiFi.localIP().toString();
  h += F(" : ");
  h += String(cfg.mqttPort ? cfg.mqttPort : 1883);
  h += F("</div>"
    "<label>Listen-Port</label><input type='text' id='mqport' value='");
  h += String(cfg.mqttPort ? cfg.mqttPort : 1883);
  h += F("'><label>User <small>(Tasmota muss diesen nutzen; leer = offen)</small></label>"
    "<input type='text' id='mquser' value='");
  h += cfg.mqttUser;
  h += F("'><label>Passwort <small>(leer = unverändert)</small></label>"
    "<input type='password' id='mqpass' placeholder='********'>"
    "<label>Anzeige-Name <small>(überschreibt Gerätenamen)</small></label>"
    "<input type='text' id='mqname' value='");
  h += cfg.mqttName;
  h += F("'></div>");

  // Display
  h += F("<div class='card'><h3>DISPLAY</h3>"
    "<label>Helligkeit</label>"
    "<input type='range' id='br' min='5' max='100' value='");
  h += String(cfg.brightness);
  h += F("' oninput='live(this.value)'></div>");

  // Aktionen
  h += F("<div class='card'>"
    "<button class='btn' onclick='save()'>SPEICHERN &amp; NEUSTART</button>"
    "<div class='msg' id='status'></div>"
    "<button class='btn btnr' onclick='reset()'>WERKSRESET</button>"
    "</div>");

  // JavaScript
  h += F("<script>"
    "function scan(){document.getElementById('nets').innerHTML='...';"
    "fetch('/scan').then(r=>r.json()).then(a=>{var o='';a.forEach(n=>{"
    "o+=\"<div class='net' onclick=\\\"document.getElementById('ssid').value='\"+n.s.replace(/'/g,\"\")+\"'\\\">\"+n.s+\"<span>\"+(n.l?'🔒 ':'')+n.r+\"dBm</span></div>\";});"
    "document.getElementById('nets').innerHTML=o;});}"
    "function live(v){fetch('/live?b='+v);}"
    "function save(){var q='/savecfg?ssid='+encodeURIComponent(document.getElementById('ssid').value)"
    "+'&pass='+encodeURIComponent(document.getElementById('pass').value)"
    "+'&owkey='+encodeURIComponent(document.getElementById('owkey').value)"
    "+'&owcity='+encodeURIComponent(document.getElementById('owcity').value)"
    "+'&owctry='+encodeURIComponent(document.getElementById('owctry').value)"
    "+'&mqport='+encodeURIComponent(document.getElementById('mqport').value)"
    "+'&mquser='+encodeURIComponent(document.getElementById('mquser').value)"
    "+'&mqpass='+encodeURIComponent(document.getElementById('mqpass').value)"
    "+'&mqname='+encodeURIComponent(document.getElementById('mqname').value)"
    "+'&br='+document.getElementById('br').value;"
    "document.getElementById('status').innerText='Speichere & starte neu...';"
    "fetch(q);}"
    "function reset(){if(confirm('Wirklich alles löschen?')){fetch('/reset');"
    "document.getElementById('status').innerText='Werksreset...';}}"
    "</script></body></html>");
  return h;
}

inline void cpHandleRoot()  { server.send(200, "text/html; charset=utf-8", cpConfigPageHTML()); }

inline void cpHandleScan() {
  int n = WiFi.scanNetworks();
  String j = "[";
  for (int i = 0; i < n && i < 15; i++) {
    if (i) j += ",";
    j += "{\"s\":\"" + WiFi.SSID(i) + "\",\"r\":" + String(WiFi.RSSI(i)) +
         ",\"l\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? 0 : 1) + "}";
  }
  j += "]";
  WiFi.scanDelete();
  server.send(200, "application/json", j);
}

inline void cpHandleLive() {
  if (server.hasArg("b")) applyBrightness((uint8_t)server.arg("b").toInt());
  server.send(200, "text/plain", "OK");
}

inline void cpHandleSaveConfig() {
  if (server.hasArg("ssid"))  cpCopy(cfg.ssid,      server.arg("ssid").c_str(),  sizeof(cfg.ssid));
  if (server.hasArg("pass") && server.arg("pass").length() > 0)
                              cpCopy(cfg.pass,      server.arg("pass").c_str(),  sizeof(cfg.pass));
  if (server.hasArg("owkey")) cpCopy(cfg.owKey,     server.arg("owkey").c_str(), sizeof(cfg.owKey));
  if (server.hasArg("owcity"))cpCopy(cfg.owCity,    server.arg("owcity").c_str(),sizeof(cfg.owCity));
  if (server.hasArg("owctry"))cpCopy(cfg.owCountry, server.arg("owctry").c_str(),sizeof(cfg.owCountry));
  // MQTT
  if (server.hasArg("mqhost")) cpCopy(cfg.mqttHost, server.arg("mqhost").c_str(), sizeof(cfg.mqttHost));
  if (server.hasArg("mqport")) cfg.mqttPort = (uint16_t)server.arg("mqport").toInt();
  if (server.hasArg("mquser")) cpCopy(cfg.mqttUser, server.arg("mquser").c_str(), sizeof(cfg.mqttUser));
  if (server.hasArg("mqpass") && server.arg("mqpass").length() > 0)
                               cpCopy(cfg.mqttPass, server.arg("mqpass").c_str(), sizeof(cfg.mqttPass));
  if (server.hasArg("mqtopic"))cpCopy(cfg.mqttTopic,server.arg("mqtopic").c_str(),sizeof(cfg.mqttTopic));
  if (server.hasArg("mqname")) cpCopy(cfg.mqttName, server.arg("mqname").c_str(), sizeof(cfg.mqttName));

  if (server.hasArg("br"))    cfg.brightness = (uint8_t)server.arg("br").toInt();
  saveDeviceConfig();
  server.send(200, "text/plain", "SAVED");
  cpSaveRequested = true;
}

inline void cpHandleReset() {
  server.send(200, "text/plain", "RESET");
  delay(300);
  factoryReset();
}

inline void cpAttachRoutes(WebServer& s) {
  s.on("/",        HTTP_GET, cpHandleRoot);
  s.on("/scan",    HTTP_GET, cpHandleScan);
  s.on("/live",    HTTP_GET, cpHandleLive);
  s.on("/savecfg", HTTP_GET, cpHandleSaveConfig);
  s.on("/reset",   HTTP_GET, cpHandleReset);
}

inline void cpBeginProvisioningAP() {
  WiFi.mode(WIFI_AP_STA);
  String ap = cpBuildApName();
  WiFi.softAP(ap.c_str(), CP_AP_PASSWORD);
  delay(300);
  String ip = WiFi.softAPIP().toString();

  Serial.printf("[SETUP] AP: %s  PASS: %s  IP: %s\n", ap.c_str(), CP_AP_PASSWORD, ip.c_str());
  cpShowSetupScreen(ap, ip);

  cpAttachRoutes(server);
  server.begin();

  cpSaveRequested = false;
  while (true) {
    server.handleClient();
    factoryCounterLoop();
    if (cpSaveRequested) { delay(400); ESP.restart(); }
    delay(5);
  }
}

#endif // CONFIG_PORTAL_H
