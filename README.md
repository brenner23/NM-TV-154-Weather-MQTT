# NM-TV-154 WeatherTV – Weather, Forecast & MQTT on ESP32

A PlatformIO project for the **NM-TV-154 / NerdMiner-TV-style 240×240 display** using an ESP32 and ST7789 TFT.

The device displays current OpenWeather data, a 3-day weather forecast, and Tasmota energy data via MQTT. The three screens are switched using the **capacitive touch input**.

## Features

- ESP32 with **ST7789 240×240 TFT**
- Current weather data from **OpenWeather**
- Temperature in °C
- Humidity in %
- Air pressure in hPa and cmHg
- Day and night weather icons
- Date and time via NTP
- Automatic daylight-saving / standard-time handling for Central Europe
- **3-day weather forecast**
- Daily minimum and maximum temperature
- Rain probability
- Weather icon for each forecast day
- Forecast cache stored in NVS
- Current-weather cache stored in NVS
- Automatic weather update every 15 minutes
- Automatic forecast update every 2 hours
- **Integrated MQTT broker** on the ESP32 using PicoMQTT
- Receives Tasmota energy data
- Displays current power, today's consumption, yesterday's consumption, and total consumption
- Displays Tasmota ON/OFF status
- Optional MQTT username and password
- Configurable display name for the MQTT device
- Web-based configuration interface
- Wi-Fi scan during setup
- OpenWeather configuration in the browser
- Display brightness adjustable in the browser
- Settings permanently stored in ESP32 NVS
- Configuration reset / factory reset
- OTA firmware updates over Wi-Fi
- OTA progress displayed directly on the TFT
- Wi-Fi watchdog with automatic restart after prolonged connection loss
- Capacitive touch for switching screens
- Touch debounce / filtering to reduce false triggers

---

## The Three Screens

A short touch on the capacitive touch area cycles through:

```text
Current Weather
      ↓
3-Day Forecast
      ↓
MQTT / Tasmota
      ↓
Current Weather
```

### 1. Current Weather

The main screen shows:

- Date
- Weekday
- Time
- Current weather icon
- Temperature
- Humidity
- Air pressure in hPa
- Air pressure additionally in cmHg

### 2. 3-Day Forecast

For the next three days, the display shows:

- Weekday
- Maximum temperature
- Minimum temperature
- Rain probability
- Weather icon

### 3. MQTT / Tasmota

In this project, the ESP32 itself acts as an **MQTT broker**.

This allows a Tasmota device to send data directly to the display without requiring an external MQTT broker such as Mosquitto.

The following Tasmota topics are processed, among others:

```text
tele/+/SENSOR
tele/+/STATE
stat/+/RESULT
stat/+/POWER
tele/+/LWT
```

The device name is automatically read from the MQTT topic. Alternatively, a custom display name can be configured in the web interface.

---

# Hardware

## Display Pins

The current pin assignment is located in:

```text
include/pins.h
```

Current assignment:

| Function | GPIO |
|---|---:|
| TFT MOSI | 13 |
| TFT MISO | 12 |
| TFT SCK / CLK | 14 |
| TFT CS | 15 |
| TFT DC | 2 |
| TFT RESET | 4 |
| TFT Enable | 21 |
| TFT Backlight | 19 |
| Capacitive Touch | 32 |

Display resolution:

```text
240 × 240 pixels
```

**Note:** Display Enable and Backlight are LOW-active on this hardware.

---

# Setting the Touch Pin and Touch Sensitivity

The capacitive touch input is read through the ESP32 touch hardware.

The settings are located in:

```text
include/pins.h
```

The relevant lines are:

```cpp
#define TOUCH_CAP_PIN  32
#define TOUCH_THRESH   90
```

## Changing the Touch Pin

If the touch area is connected to a different touch-capable ESP32 GPIO on your hardware, change `TOUCH_CAP_PIN`.

Example:

```cpp
#define TOUCH_CAP_PIN  32
```

In this project, `32` means **GPIO 32**.

Not every ESP32 GPIO supports capacitive touch. The selected pin must support touch input on the ESP32 variant being used.

## Setting Touch Sensitivity / Threshold

The important setting is:

```cpp
#define TOUCH_THRESH 90
```

On the currently tested hardware, typical values were approximately:

```text
Finger not touching: about 106
Finger touching:     about 82
```

A touch is detected when the measured value becomes **lower than `TOUCH_THRESH`**.

A useful threshold is normally somewhere between the idle value and the touched value.

Example:

```text
Idle:       106
Touched:     82
Middle:      94
```

In this case, you could try:

```cpp
#define TOUCH_THRESH 94
```

The current project default is:

```cpp
#define TOUCH_THRESH 90
```

---

# Touch Does Not Work Immediately? – Calibration / Troubleshooting

If touching the case does not switch screens reliably, you can output the raw touch values to the serial monitor.

Open:

```text
src/main.cpp
```

and change:

```cpp
#define DEBUG_TOUCH 0
```

to:

```cpp
#define DEBUG_TOUCH 1
```

Then rebuild and flash the firmware.

Open the serial monitor at **115200 baud**.

The output will look similar to:

```text
touch=  106   min=  104  max=  108   -> mitte=106
touch=   82   min=   82  max=  108   -> mitte=95
```

Watch the values for several seconds without touching the device. Then touch the capacitive touch area several times with your finger.

Set `TOUCH_THRESH` in `include/pins.h` to a value between the untouched and touched ranges.

### Touch does not react at all

Increase the threshold slightly.

Example:

```cpp
#define TOUCH_THRESH 95
```

### Touch triggers by itself

Lower the threshold slightly.

Example:

```cpp
#define TOUCH_THRESH 88
```

The firmware already includes additional touch filtering:

- three valid touch measurements in a row
- at least 20 ms between evaluated measurements
- 1.5 second cooldown after changing screens
- additional release hysteresis

This greatly reduces false triggering caused by short interference or drifting touch values.

After successful calibration, it is recommended to disable touch debugging again:

```cpp
#define DEBUG_TOUCH 0
```

---

# First Start / Initial Setup

If no Wi-Fi configuration is stored in NVS, the device starts its own Wi-Fi access point.

The SSID begins with:

```text
ESP32-WeatherTV-
```

The current default password for the setup access point is:

```text
12345678
```

Connect to this Wi-Fi network and open the IP address shown on the display in a browser.

The setup page allows configuration of:

- Wi-Fi SSID
- Wi-Fi password
- OpenWeather API key
- Location / city
- Country
- MQTT port
- MQTT username
- MQTT password
- MQTT display name
- Display brightness

After selecting **Save & Restart**, the settings are stored in ESP32 NVS.

---

# Web Configuration

After a successful Wi-Fi connection, the ESP32 runs a web server on port 80.

Open:

```text
http://DISPLAY-IP/
```

Example:

```text
http://192.168.0.45/
```

The actual IP address is shown during startup on the display and/or in the serial monitor.

The web interface allows the most important settings to be changed without reflashing the firmware.

Display brightness is applied directly when moving the brightness slider.

---

# OpenWeather Setup

An OpenWeather API key is required for weather information.

Enter the following in the web interface:

```text
API Key
Location
Country
```

Example:

```text
Location: Frelsdorf
Country: DE
```

The software uses metric units and German weather data.

Current weather is updated approximately every **15 minutes**.

The 3-day forecast is updated approximately every **2 hours**.

Downloaded weather and forecast data is cached in NVS and remains available after a restart.

---

# MQTT / Tasmota Setup

In this project version, the ESP32 itself is the MQTT broker.

The display IP address is shown in the web interface.

In the Tasmota MQTT settings, enter:

```text
Host:     IP address of the NM-TV-154
Port:     1883
User:     same as configured in the NM-TV web interface
Password: same as configured in the NM-TV web interface
```

If no MQTT username is configured on the display, the broker accepts connections without username/password authentication.

The default port is:

```text
1883
```

The MQTT screen can display, among other values:

- Current power in watts
- Energy consumption today in kWh
- Energy consumption yesterday in kWh
- Total energy consumption in kWh
- Relay status ON/OFF
- Online/offline status using Tasmota LWT

---

# NTP / Time

The clock is synchronized using NTP.

Configured servers:

```text
pool.ntp.org
time.nist.gov
```

The timezone is configured for Central Europe and automatically handles daylight-saving and standard time.

---

# OTA – Firmware Update over Wi-Fi

In addition to normal USB flashing, firmware can be updated over Wi-Fi using ArduinoOTA.

PlatformIO provides two environments:

```text
usb
ota
```

## USB Upload

```bash
pio run -e usb -t upload
```

## OTA Upload

First enter the current IP address of the display in `platformio.ini`:

```ini
[env:ota]
upload_protocol = espota
upload_port = 192.168.0.45
```

Then run:

```bash
pio run -e ota -t upload
```

During an OTA update, the display shows a progress bar and percentage.

## Important: OTA Partition Layout

The firmware is too large for small OTA application slots.

If you encounter an error such as:

```text
OTA_BEGIN_ERROR
```

use a partition layout with sufficiently large OTA application slots.

The project is already configured with:

```ini
board_build.partitions = min_spiffs.csv
```

After changing the partition table, the device must be flashed via USB at least once.

---

# PlatformIO

The project uses the Arduino framework for ESP32.

Important libraries include:

```text
Adafruit GFX Library
Adafruit ST7735 and ST7789 Library
Adafruit BusIO
ArduinoJson 7
PicoMQTT
```

Default environment:

```text
usb
```

Serial speed:

```text
115200 baud
```

Build the project:

```bash
pio run
```

Build and upload via USB:

```bash
pio run -e usb -t upload
```

Open the serial monitor:

```bash
pio device monitor
```

---

# Security Before Publishing on GitHub

Before publishing the project in a public GitHub repository, check the complete source tree for credentials and private configuration.

Do **not** publish real:

- Wi-Fi SSIDs or passwords
- OpenWeather API keys
- MQTT usernames or passwords
- OTA passwords
- private IP addresses if you do not want them visible
- other API tokens or credentials

Replace private values with placeholders or move them into a local configuration file that is excluded using `.gitignore`.

Example:

```cpp
const char* wifiPassword = "YOUR_WIFI_PASSWORD";
const char* weatherApiKey = "YOUR_OPENWEATHER_API_KEY";
```

---

# License

This project is licensed under the MIT License.

See the `LICENSE` file for details.

---

# Deutsche Version

# NM-TV-154 WeatherTV – Wetter, Forecast & MQTT auf ESP32

Ein PlatformIO-Projekt für das **NM-TV-154 / NerdMiner-TV ähnliche 240×240-Display** mit ESP32 und ST7789.

Das Gerät zeigt Wetterdaten von OpenWeather, eine 3-Tage-Vorhersage und Tasmota-Energiedaten über MQTT an. Zwischen den drei Ansichten wird über den **kapazitiven Touch-Eingang** umgeschaltet.

## Features

- ESP32 mit **ST7789 240×240 TFT**
- Live-Wetter von **OpenWeather**
- Temperatur in °C
- Luftfeuchtigkeit in %
- Luftdruck in hPa und cmHg
- Wetter-Icons für Tag und Nacht
- Datum und Uhrzeit über NTP
- automatische Sommer-/Winterzeit für Deutschland / Mitteleuropa
- **3-Tage-Wettervorhersage**
- Tages-Minimum und Tages-Maximum
- Regenwahrscheinlichkeit
- Wetter-Icon je Prognosetag
- Forecast-Cache im NVS
- Live-Wetter-Cache im NVS
- Wetter-Aktualisierung automatisch alle 15 Minuten
- Forecast-Aktualisierung automatisch alle 2 Stunden
- **integrierter MQTT-Broker** auf dem ESP32 über PicoMQTT
- Empfang von Tasmota-Energiedaten
- Anzeige von Leistung, Tagesverbrauch, Vortagesverbrauch und Gesamtverbrauch
- Anzeige von Tasmota ON/OFF-Status
- MQTT-Benutzername und Passwort optional
- frei einstellbarer Anzeigename für das MQTT-Gerät
- Web-Konfigurationsoberfläche
- WLAN-Scan im Setup
- OpenWeather-Konfiguration über Browser
- Display-Helligkeit über Browser einstellbar
- Einstellungen dauerhaft im ESP32-NVS gespeichert
- Konfigurations-Reset / Werksreset
- OTA-Updates über WLAN
- OTA-Fortschrittsanzeige direkt auf dem TFT
- WLAN-Watchdog mit automatischem Neustart bei längerem Verbindungsverlust
- kapazitiver Touch zum Wechseln der Anzeige
- Touch-Entprellung gegen Fehlauslösungen

---

## Die drei Ansichten

Ein kurzer Fingertipp auf die Touch-Fläche schaltet zyklisch durch:

```text
Live-Wetter
    ↓
3-Tage-Forecast
    ↓
MQTT / Tasmota
    ↓
Live-Wetter
```

### 1. Live-Wetter

Die Hauptansicht zeigt:

- Datum
- Wochentag
- Uhrzeit
- aktuelles Wetter-Icon
- Temperatur
- Luftfeuchtigkeit
- Luftdruck in hPa
- Luftdruck zusätzlich in cmHg

### 2. 3-Tage-Forecast

Für die nächsten drei Tage werden dargestellt:

- Wochentag
- maximale Temperatur
- minimale Temperatur
- Regenwahrscheinlichkeit
- Wetter-Icon

### 3. MQTT / Tasmota

Der ESP32 arbeitet in diesem Projekt selbst als **MQTT-Broker**.

Ein Tasmota-Gerät kann deshalb direkt Daten an das Display senden, ohne dass zwingend ein externer MQTT-Broker wie Mosquitto benötigt wird.

Unter anderem werden diese Tasmota-Topics verarbeitet:

```text
tele/+/SENSOR
tele/+/STATE
stat/+/RESULT
stat/+/POWER
tele/+/LWT
```

Der Gerätename wird automatisch aus dem Topic gelesen. Alternativ kann im Webinterface ein eigener Anzeigename gesetzt werden.

---

# Hardware

## Display-Pins

Die aktuelle Pinbelegung befindet sich in:

```text
include/pins.h
```

Aktuelle Belegung:

| Funktion | GPIO |
|---|---:|
| TFT MOSI | 13 |
| TFT MISO | 12 |
| TFT SCK / CLK | 14 |
| TFT CS | 15 |
| TFT DC | 2 |
| TFT RESET | 4 |
| TFT Enable | 21 |
| TFT Backlight | 19 |
| Kapazitiver Touch | 32 |

Das Display hat eine Auflösung von:

```text
240 × 240 Pixel
```

**Hinweis:** Display-Enable und Backlight sind bei dieser Hardware LOW-aktiv.

---

# Touch-Pin und Touch-Empfindlichkeit einstellen

Der kapazitive Touch wird über den ESP32-Touch-Eingang gelesen.

Die Einstellungen befinden sich in:

```text
include/pins.h
```

Relevant sind diese beiden Zeilen:

```cpp
#define TOUCH_CAP_PIN  32
#define TOUCH_THRESH   90
```

## Touch-Pin ändern

Falls die Touch-Fläche bei einer anderen Hardware an einem anderen ESP32-Touch-Pin angeschlossen ist, muss `TOUCH_CAP_PIN` angepasst werden.

Beispiel:

```cpp
#define TOUCH_CAP_PIN  32
```

`32` bedeutet in diesem Projekt: **GPIO 32**.

Nicht jeder ESP32-Pin unterstützt kapazitives Touch. Der verwendete Pin muss ein Touch-fähiger Eingang des jeweiligen ESP32 sein.

## Kapazität / Touch-Schwelle einstellen

Entscheidend ist:

```cpp
#define TOUCH_THRESH 90
```

Bei der aktuell getesteten Hardware wurden ungefähr folgende Werte gemessen:

```text
Finger nicht auf der Fläche: etwa 106
Finger auf der Fläche:       etwa 82
```

Der Touch gilt als gedrückt, wenn der Messwert **kleiner als `TOUCH_THRESH`** wird.

Eine sinnvolle Schwelle liegt zwischen Ruhewert und Berührungswert.

Beispiel:

```text
Ruhe:       106
Berührt:     82
Mittelwert:  94
```

Dann kann beispielsweise getestet werden mit:

```cpp
#define TOUCH_THRESH 94
```

Die im Projekt aktuell eingestellte Schwelle ist:

```cpp
#define TOUCH_THRESH 90
```

---

# Touch funktioniert nicht sofort? – Kalibrierung

Falls das Umschalten per Finger nicht oder nur unzuverlässig funktioniert, können die echten Rohwerte über den seriellen Monitor ausgegeben werden.

Öffne:

```text
src/main.cpp
```

und ändere:

```cpp
#define DEBUG_TOUCH 0
```

zu:

```cpp
#define DEBUG_TOUCH 1
```

Danach neu kompilieren und flashen.

Den seriellen Monitor mit **115200 Baud** öffnen.

Die Ausgabe sieht ungefähr so aus:

```text
touch=  106   min=  104  max=  108   -> mitte=106
touch=   82   min=   82  max=  108   -> mitte=95
```

Jetzt mehrere Sekunden ohne Berührung beobachten und anschließend mehrmals mit dem Finger auf die Touch-Fläche drücken.

Danach `TOUCH_THRESH` in `include/pins.h` auf einen Wert zwischen beiden Bereichen setzen.

### Wenn Touch gar nicht reagiert

Dann den Schwellwert etwas **höher** setzen.

Beispiel:

```cpp
#define TOUCH_THRESH 95
```

### Wenn Touch von alleine auslöst

Dann den Schwellwert etwas **niedriger** setzen.

Beispiel:

```cpp
#define TOUCH_THRESH 88
```

Die Software enthält zusätzlich bereits eine Entprellung:

- drei gültige Touch-Messungen hintereinander
- mindestens 20 ms Abstand zwischen den ausgewerteten Messungen
- 1,5 Sekunden Cooldown nach einem Ansichtswechsel
- zusätzliche Hysterese beim Loslassen

Dadurch werden kurze Störungen und Drift weitgehend unterdrückt.

Nach erfolgreicher Kalibrierung empfiehlt es sich, die Debug-Ausgabe wieder abzuschalten:

```cpp
#define DEBUG_TOUCH 0
```

---

# Erstinbetriebnahme

Wenn noch keine WLAN-Konfiguration im NVS gespeichert ist, startet das Gerät einen eigenen WLAN-Access-Point.

Der Name beginnt mit:

```text
ESP32-WeatherTV-
```

Das Standardpasswort des Setup-Access-Points ist derzeit:

```text
12345678
```

Nach dem Verbinden mit diesem WLAN die auf dem Display angezeigte IP-Adresse im Browser öffnen.

Im Setup können unter anderem eingestellt werden:

- WLAN-SSID
- WLAN-Passwort
- OpenWeather API-Key
- Ort
- Land
- MQTT-Port
- MQTT-Benutzername
- MQTT-Passwort
- MQTT-Anzeigename
- Display-Helligkeit

Nach **Speichern & Neustart** werden die Daten im NVS des ESP32 gespeichert.

---

# Web-Konfiguration

Nach erfolgreicher WLAN-Verbindung läuft auf dem ESP32 ein Webserver auf Port 80.

Im Browser öffnen:

```text
http://IP-DES-DISPLAYS/
```

Beispiel:

```text
http://192.168.0.45/
```

Die tatsächliche Adresse wird beim Start auf dem Display bzw. im seriellen Monitor angezeigt.

Über die Weboberfläche können die wichtigsten Einstellungen ohne erneutes Flashen geändert werden.

Die Helligkeit wird beim Verschieben des Reglers direkt übernommen.

---

# OpenWeather einrichten

Für die Wetteranzeige wird ein API-Key von OpenWeather benötigt.

Im Webinterface eintragen:

```text
API Key
Ort
Land
```

Beispiel:

```text
Ort: Frelsdorf
Land: DE
```

Die Software verwendet metrische Einheiten und deutsche Wetterdaten.

Die aktuellen Wetterdaten werden etwa alle **15 Minuten** aktualisiert.

Die 3-Tage-Vorhersage wird etwa alle **2 Stunden** aktualisiert.

Bereits geladene Wetter- und Forecast-Daten werden im NVS gespeichert und stehen nach einem Neustart weiterhin als Cache zur Verfügung.

---

# MQTT / Tasmota einrichten

Der ESP32 ist in dieser Projektversion selbst der MQTT-Broker.

Die IP-Adresse des Displays wird im Webinterface angezeigt.

In Tasmota unter den MQTT-Einstellungen eintragen:

```text
Host:     IP-Adresse des NM-TV-154
Port:     1883
User:     wie im NM-TV-Webinterface eingestellt
Password: wie im NM-TV-Webinterface eingestellt
```

Wenn im Display kein MQTT-Benutzername hinterlegt wurde, akzeptiert der Broker Verbindungen ohne Benutzername/Passwort.

Der Standardport ist:

```text
1883
```

Die MQTT-Seite kann unter anderem folgende Energiewerte anzeigen:

- aktuelle Leistung in Watt
- Verbrauch heute in kWh
- Verbrauch gestern in kWh
- Gesamtverbrauch in kWh
- Relaisstatus ON/OFF
- Online-/Offline-Status über Tasmota LWT

---

# NTP / Uhrzeit

Die Uhr wird über NTP synchronisiert.

Verwendete Server:

```text
pool.ntp.org
time.nist.gov
```

Die Zeitzone ist für Mitteleuropa eingerichtet und berücksichtigt Sommer- und Winterzeit automatisch.

---

# OTA – Update über WLAN

Neben dem normalen USB-Upload kann die Firmware per ArduinoOTA über WLAN aktualisiert werden.

PlatformIO besitzt dafür zwei Environments:

```text
usb
ota
```

## USB-Upload

```bash
pio run -e usb -t upload
```

## OTA-Upload

Vorher in `platformio.ini` die aktuelle IP-Adresse des Displays eintragen:

```ini
[env:ota]
upload_protocol = espota
upload_port = 192.168.0.45
```

Danach:

```bash
pio run -e ota -t upload
```

Während eines OTA-Updates zeigt das Display einen Fortschrittsbalken und den Prozentwert an.

## Wichtig: Partitionierung für OTA

Die Firmware ist für kleine OTA-App-Slots zu groß.

Bei Problemen wie:

```text
OTA_BEGIN_ERROR
```

muss ein Partitionsschema mit ausreichend großem OTA-App-Slot verwendet werden.

Im Projekt ist bereits konfiguriert:

```ini
board_build.partitions = min_spiffs.csv
```

Nach einer Änderung der Partitionstabelle muss mindestens einmal per USB geflasht werden.

---

# PlatformIO

Das Projekt verwendet das Arduino-Framework für ESP32.

Wichtige Bibliotheken:

```text
Adafruit GFX Library
Adafruit ST7735 and ST7789 Library
Adafruit BusIO
ArduinoJson 7
PicoMQTT
```

Standard-Environment:

```text
usb
```

Serielle Geschwindigkeit:

```text
115200 Baud
```

Projekt kompilieren:

```bash
pio run
```

Kompilieren und per USB hochladen:

```bash
pio run -e usb -t upload
```

Seriellen Monitor öffnen:

```bash
pio device monitor
```

---

# WLAN-Watchdog

Die WLAN-Verbindung wird regelmäßig überprüft.

Wenn das WLAN verloren geht und nicht wieder vorhanden ist, startet der ESP32 automatisch neu.

Die Prüfung erfolgt derzeit ungefähr einmal pro Minute.

---

# Werksreset

Über die Weboberfläche kann ein Werksreset ausgelöst werden.

Dabei werden unter anderem gelöscht:

- WLAN-Konfiguration
- OpenWeather-Konfiguration
- MQTT-Konfiguration
- gespeicherte Wetterdaten
- gespeicherte Forecast-Daten
- Boot-/Reset-Zähler

Danach startet das Gerät neu und kann erneut eingerichtet werden.

Im Quellcode existiert zusätzlich ein Hardware-Reset-Modus über wiederholte Neustarts. Dieser ist standardmäßig deaktiviert:

```cpp
#define HW_RESET_MODE_ENABLED 0
```

---

# Troubleshooting

## Display bleibt schwarz

Prüfen:

- richtige Pinbelegung in `include/pins.h`
- `TFT_EN_PIN`
- Backlight-Pin
- SPI-Pins
- Versorgungsspannung
- richtige Displayvariante ST7789 240×240

Bei dieser Hardware muss `TFT_EN_PIN` auf LOW gezogen werden.

## Display steht auf dem Kopf

Die Rotation wird in `src/main.cpp` gesetzt:

```cpp
tft.setRotation(2);
```

Falls eine andere Displaymontage verwendet wird, kann der Wert angepasst werden.

## Wetter bleibt auf alten Werten

Prüfen:

- WLAN-Verbindung
- OpenWeather API-Key
- Ort und Land
- Internetzugang des ESP32
- seriellen Monitor auf HTTP-/WLAN-Fehler prüfen

## Forecast erscheint nicht

Der Forecast wird gecacht und normalerweise nur etwa alle zwei Stunden neu geladen.

Außerdem müssen gültige OpenWeather-Zugangsdaten vorhanden sein.

## MQTT zeigt nur `Wait...`

Prüfen:

- Tasmota MQTT aktiviert
- als MQTT-Host die IP des Displays eingetragen
- Port stimmt mit dem Webinterface überein
- Benutzername und Passwort stimmen überein
- Tasmota sendet `tele/.../SENSOR` bzw. `tele/.../STATE`
- seriellen Monitor öffnen, da MQTT-Ereignisse im Projekt protokolliert werden können

## Touch reagiert nicht

1. `DEBUG_TOUCH` auf `1` setzen.
2. Seriellen Monitor mit 115200 Baud öffnen.
3. Werte ohne Finger notieren.
4. Werte mit Finger auf der Touch-Fläche notieren.
5. `TOUCH_THRESH` zwischen beide Bereiche setzen.
6. Neu flashen und testen.

## Touch schaltet ständig von alleine

`TOUCH_THRESH` etwas reduzieren.

Beispiel:

```cpp
#define TOUCH_THRESH 88
```

Zusätzlich prüfen, ob eine zu große Metallfläche, ein ungünstiges Kabel, Störungen der Spannungsversorgung oder ein schwankendes Massepotential den kapazitiven Eingang beeinflussen.

---

# Sicherheit vor Veröffentlichung auf GitHub

**Vor einem öffentlichen GitHub-Upload unbedingt prüfen.**

In privaten Entwicklungsständen können sich echte Daten befinden, zum Beispiel:

- WLAN-SSID
- WLAN-Passwort
- OpenWeather API-Key
- OTA-Passwort
- MQTT-Zugangsdaten

Solche Daten sollten **nicht in ein öffentliches Repository hochgeladen werden**.

Insbesondere sollte `include/credentials.h` entweder nur Platzhalter enthalten oder über `.gitignore` ausgeschlossen werden.

Eine mögliche öffentliche Vorlage wäre beispielsweise:

```cpp
#define USE_FALLBACK_CREDENTIALS 0

const char* WIFI_SSID = "";
const char* WIFI_PASS = "";
const char* OPENWEATHER_API_KEY = "";
const char* OPENWEATHER_CITY = "";
const char* OPENWEATHER_COUNTRY = "DE";
const char* OPENWEATHER_UNITS = "metric";
const char* OPENWEATHER_LANG = "de";
```

Auch das OTA-Passwort sollte vor Veröffentlichung geändert oder in eine nicht versionierte Konfigurationsdatei ausgelagert werden.

**Wichtig:** Wenn ein echter API-Key oder ein Passwort bereits öffentlich zu GitHub gepusht wurde, reicht späteres Löschen aus der Datei nicht unbedingt aus, da die Daten weiterhin in der Git-Historie stehen können. In diesem Fall das betroffene Passwort bzw. den API-Key ersetzen.

---

# Projektstruktur

```text
.
├── platformio.ini
├── src/
│   └── main.cpp
└── include/
    ├── config_portal.h
    ├── credentials.h
    ├── forecast.h
    ├── icons.h
    ├── mqtt.h
    ├── ota.h
    └── pins.h
```

Je nach Projektstand können zusätzliche Font- und Icon-Dateien vorhanden sein.

---

# Kurzfassung Touch-Kalibrierung

Wenn nur eine Sache nach dem Flashen nicht sofort funktioniert, ist es meistens die individuelle Touch-Schwelle.

```cpp
// include/pins.h
#define TOUCH_CAP_PIN 32
#define TOUCH_THRESH  90
```

Zum Messen:

```cpp
// src/main.cpp
#define DEBUG_TOUCH 1
```

Dann Ruhewert und Fingerwert im seriellen Monitor vergleichen und die Schwelle dazwischen setzen.

**Niedrigerer `TOUCH_THRESH` = weniger empfindlich.**  
**Höherer `TOUCH_THRESH` = empfindlicher.**

---

## Lizenz

Für dieses Projekt ist derzeit keine Lizenzdatei definiert. Wer das Repository öffentlich bereitstellt und anderen ausdrücklich Nutzung, Änderung oder Weitergabe erlauben möchte, sollte eine passende `LICENSE`-Datei ergänzen.
