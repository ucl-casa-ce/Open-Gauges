/*
  Open Gauges — one firmware for every stepper gauge
  Board: ESP32-C3 Super Mini (Arduino core for ESP32 v3.x)
  Motor: 28BYJ-48 + ULN2003 driver   Homing: KY-003 hall sensor + 5 mm magnet
  LEDs:  WS2812 / NeoPixel strip around the rim

  Gauges: wind direction, wind speed, air pressure, temperature, daily rainfall.
  Same wiring for all of them. Which gauge it is gets chosen from your phone.

  NOTHING TO EDIT IN THIS FILE.
  - First power-up: the ring turns CYAN and the gauge makes its own Wi-Fi network
    "OpenGauge-xxxx" (password: opengauge). Join it on your phone and the setup page
    opens (if it doesn't, browse to 192.168.4.1). Choose the gauge type and your Wi-Fi,
    check the weather feed, and press Save.
  - Settings page: with your phone on the same Wi-Fi, browse to
    http://opengauge-<type>-xxxx.local (or the gauge's IP, shown in the Serial Monitor).
    Backlight, weather feed (with a live check that data is arriving), pointer tuning
    and gauge type. The page is served for 10 minutes after start-up or after it was
    last used; restart the gauge or press its BOOT button to open it again.
  - Back to the full setup page (e.g. a new Wi-Fi network): hold BOOT while the ring is
    RED at power-up. If the saved Wi-Fi can't be found at start-up, the setup page opens
    for 3 minutes by itself, then the gauge keeps trying the saved network.
  - Wireless updates: once on your Wi-Fi the gauge shows up in the Arduino IDE under
    Tools > Port as a network port. Upload as normal; the ring fills green.
    (The IDE asks for a password even if you didn't set one: type anything.)
  - Homing failure: if the magnet isn't found within two turns the ring flashes red,
    the gauge carries on, LED 0 stays red as a warning, and it retries every hour.

  Start-up colours: RED started, CYAN setup page, BLUE Wi-Fi, YELLOW homing,
  PURPLE self-test, then the backlight (or dark) when ready.

  Libraries (Library Manager): AccelStepper, PubSubClient, ArduinoJson (v7),
                               Adafruit NeoPixel, WiFiManager (by tzapu, 2.0.17+)
  Tools menu: Board "ESP32C3 Dev Module", USB CDC On Boot "Enabled",
              Partition Scheme: one "with OTA" — default 1.2MB APP, or
              "Minimal SPIFFS (1.9MB APP with OTA)" if the sketch is too big.
*/

#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <AccelStepper.h>
#include <Adafruit_NeoPixel.h>
#include <time.h>
#include <initializer_list>

// ===========================================================================
// HARDWARE (the same for every gauge)
// ===========================================================================
#define IN1_PIN   4     // ULN2003 IN1
#define IN2_PIN   3     // ULN2003 IN2
#define IN3_PIN   1     // ULN2003 IN3
#define IN4_PIN   0     // ULN2003 IN4
#define HALL_PIN  7     // KY-003 S
#define HALL_PWR  5     // optional: KY-003 + here to power it only while homing
#define LED_PIN   10    // LED strip DIN
#define SETUP_BTN_PIN  9     // on-board BOOT button

#define NUM_LEDS          84
#define WARN_LED          0       // lit red while homing has failed
#define HIGHLIGHT_BRIGHT  255     // trend / day's-max LEDs
#define TREND_FADE_MIN    30      // lowest point of the pulse for a rapid change
const uint8_t backlightRGB[3] = {255, 255, 255};
const float   pulsePeriodS    = 2.0;

// Where the LEDs sit (used to light the LED beside a value, e.g. the day's max).
// LED 0 is at the gap just right of 6 o'clock and the strip runs anticlockwise.
const float led0BearingDeg = 148;     // bearing of LED 0, degrees clockwise from 12 o'clock
const float ledsPerMetre   = 144.0;
const float stripRadiusMm  = 95.0;
const int   ledDirection   = -1;      // -1 = anticlockwise from LED 0
const float degPerLed      = 360.0 / (2.0 * PI * stripRadiusMm / 1000.0 * ledsPerMetre);

long  stepsPerRevolution = 4096;      // measured at start-up
float stepsPerDegree     = stepsPerRevolution / 360.0;
const float homingSpeed  = 300.0;
const float runSpeed     = 400.0;
const float runAccel     = 150.0;
const unsigned long rehomeIntervalMs = 6UL * 60UL * 60UL * 1000UL;  // every 6 h
const unsigned long rehomeRetryMs    = 60UL * 60UL * 1000UL;        // hourly after a failure
const float duskSunAngle = -0.833;    // -0.833 = sunset, -6 = civil dusk

#define SETUP_AP_PASSWORD "opengauge"
#define DEMO_FEED "personal/ucfnaps/downhamweather/"   // Andy's station, as a working demo

// ===========================================================================
// GAUGE TYPES
// ===========================================================================
enum GaugeType { G_NONE = -1, G_WINDDIR = 0, G_WINDSPEED, G_PRESSURE, G_TEMP, G_RAIN, G_COUNT };
enum Extra { EX_NONE, EX_TREND, EX_DAYMAX };

struct KeyDef { const char* key; const char* unit; };   // unit nullptr = "bare" key

// JSON keys to look for, best first
const KeyDef dirKeys[]   = {{"windDir","deg"},{"wind_dir","deg"},{"windDir_degree_compass","deg"},{"winddir","deg"}};
const KeyDef speedKeys[] = {{"windgust1min",nullptr},{"windGust1min",nullptr},{"windGust_mph","mph"},
                            {"windGust_kph","kph"},{"windGust_mps","mps"},{"windGust_knot","knot"},
                            {"windGust",nullptr},{"value",nullptr}};
const KeyDef presKeys[]  = {{"barometer_mbar","hPa"},{"barometer_hPa","hPa"},{"barometer_inHg","inHg"},
                            {"barometer",nullptr},{"pressure_mbar","hPa"},{"pressure_hPa","hPa"},
                            {"pressure_inHg","inHg"},{"pressure",nullptr}};
const KeyDef tempKeys[]  = {{"outTemp_C","C"},{"outTemp_degree_C","C"},{"outTemp_F","F"},
                            {"outTemp_degree_F","F"},{"outTemp",nullptr},{"temperature_C","C"},
                            {"temperature_F","F"},{"temperature",nullptr}};
const KeyDef rainKeys[]  = {{"dayRain_mm","mm"},{"dayRain_cm","cm"},{"dayRain_in","in"},
                            {"dayRain",nullptr},{"day_rain",nullptr}};

// Plain-number topics: these name endings give the units (e.g. ".../dayRain_cm")
const char* const speedSuffixes[] = {"_kph", "_mps", "_knot"};
const char* const tempSuffixes[]  = {"_F"};
const char* const rainSuffixes[]  = {"_cm", "_in"};

const int presRising[]  = {79, 80, 82, 83, 1, 2};
const int presFalling[] = {70, 71, 72, 73, 74};
const int tempRising[]  = {83, 1, 2, 3};
const int tempFalling[] = {68, 69, 70, 71, 72, 73, 74};

struct Profile {
  const char* name;          // short name: hostnames, setup network
  const char* title;
  const char* unit;          // shown on the settings page
  bool  full360;             // wind direction: whole circle, shortest way round
  int   nPoints;             // scale breakpoints: value -> bearing (deg clockwise from 12)
  float val[4];
  float deg[4];
  float minValid, maxValid;  // readings outside this are ignored
  float deadband;            // don't move for smaller changes
  float smoothing;           // 1 = none, 0.2 = light smoothing
  const char* topic;         // default topic
  float homeOffset, backlash;// defaults
  const char* unitChoices;   // units of a bare JSON key: "value:label,value:label"
  const char* defaultUnits;
  const char* plainUnits;    // units of a plain-number topic with no unit ending
  const KeyDef* keys; int nKeys;
  const char* const* suffixes; int nSuffixes;
  Extra extra;
  float trendOn, trendOff, trendFast;      // 3-hour change thresholds
  const int* risingLeds; int nRising;
  const int* fallingLeds; int nFalling;
  uint8_t risingRGB[3], fallingRGB[3];
};

#define COUNT_OF(a) (int)(sizeof(a) / sizeof(a[0]))
const Profile profiles[G_COUNT] = {
  { "Wind", "Wind direction", "°", true, 2, {0, 360}, {0, 360}, -360, 720, 2.0, 1.0,
    DEMO_FEED "windDir", 6.0, 4.0, "deg:degrees", "deg", "deg",
    dirKeys, COUNT_OF(dirKeys), nullptr, 0, EX_NONE, 0, 0, 0, nullptr, 0, nullptr, 0, {0,0,0}, {0,0,0} },
  { "Speed", "Wind speed", "mph", false, 2, {0, 40}, {-135, 135}, 0, 200, 0.2, 1.0,
    DEMO_FEED "windgust1min", 17.0, 2.0, "mph:mph,kph:km/h,mps:m/s,knot:knots", "mph", "mph",
    speedKeys, COUNT_OF(speedKeys), speedSuffixes, COUNT_OF(speedSuffixes), EX_DAYMAX, 0, 0, 0,
    nullptr, 0, nullptr, 0, {0,0,0}, {0,0,0} },
  { "Pressure", "Air pressure", "hPa", false, 2, {950, 1050}, {-135, 135}, 870, 1090, 0.1, 0.2,
    DEMO_FEED "loop", 14.0, 4.0, "hPa:hPa (inHg is spotted automatically)", "hPa", "hPa",
    presKeys, COUNT_OF(presKeys), nullptr, 0, EX_TREND, 0.5, 0.3, 3.6,
    presRising, COUNT_OF(presRising), presFalling, COUNT_OF(presFalling), {0, 200, 60}, {255, 70, 0} },
  { "Temp", "Temperature", "°C", false, 2, {-10, 40}, {-135, 135}, -40, 60, 0.1, 0.2,
    DEMO_FEED "loop", 19.0, 4.0, "F:°F (weeWX US units),C:°C", "F", "C",
    tempKeys, COUNT_OF(tempKeys), tempSuffixes, COUNT_OF(tempSuffixes), EX_TREND, 1.0, 0.5, 4.0,
    tempRising, COUNT_OF(tempRising), tempFalling, COUNT_OF(tempFalling), {255, 70, 0}, {0, 120, 255} },
  { "Rain", "Daily rainfall", "mm", false, 4, {0, 2, 10, 50}, {-135, -94.0909, -28.6364, 135},
    0, 1000, 0.1, 1.0,
    DEMO_FEED "loop", 13.0, 4.0, "mm:mm (METRICWX),cm:cm (METRIC),in:inches (US)", "mm", "mm",
    rainKeys, COUNT_OF(rainKeys), rainSuffixes, COUNT_OF(rainSuffixes), EX_NONE, 0, 0, 0,
    nullptr, 0, nullptr, 0, {0,0,0}, {0,0,0} },
};

// ===========================================================================
// SETTINGS (saved in flash; set from the phone)
// ===========================================================================
enum LightsMode { LIGHTS_ON = 0, LIGHTS_OFF = 1, LIGHTS_DUSK = 2 };

struct Config {
  int   gaugeType      = G_NONE;
  char  mqttServer[64] = "mqtt.cetools.org";
  int   mqttPort       = 1883;
  char  mqttUser[32]   = "";
  char  mqttPass[32]   = "";
  int   lightsMode     = LIGHTS_DUSK;
  int   offHour        = 0;
  int   offMinute      = 0;
  float latitude       = 0.0;
  float longitude      = 0.0;
  char  tz[48]         = "GMT0BST,M3.5.0/1,M10.5.0";   // UK
  int   brightness     = 120;
  char  otaPass[32]    = "";
} cfg;

// Kept separately for each gauge type, so switching type doesn't lose them
struct TypeSettings {
  char  topic[128];
  char  units[6];
  float homeOffsetDeg;
  float backlashDeg;
  float trimLowDeg;          // nudge at the bottom-left end, fading to nothing at the top
  float trimHighDeg;         // nudge at the bottom-right end
} ts;

Preferences prefs;

const Profile& P() { return profiles[cfg.gaugeType < 0 ? G_RAIN : cfg.gaugeType]; }

void getStr(const char* key, char* buf, size_t len) {
  if (prefs.isKey(key)) prefs.getString(key, buf, len);
}
float getF(const char* key, float def) { return prefs.isKey(key) ? prefs.getFloat(key, def) : def; }
int   getI(const char* key, int def)   { return prefs.isKey(key) ? prefs.getInt(key, def)   : def; }

bool unitAllowed(int t, const char* u) {
  String list = String(",") + profiles[t].unitChoices;
  return list.indexOf(String(",") + u + ":") >= 0;
}

void loadTypeSettings(int t, TypeSettings& s) {
  const Profile& p = profiles[t];
  strlcpy(s.topic, p.topic, sizeof(s.topic));
  strlcpy(s.units, p.defaultUnits, sizeof(s.units));
  s.homeOffsetDeg = p.homeOffset;
  s.backlashDeg   = p.backlash;
  s.trimLowDeg = s.trimHighDeg = 0;
  char k[12];
  prefs.begin("ogauge", true);
  if (t == G_RAIN) {                           // settings from the rain-gauge trial sketch
    getStr("topic", s.topic, sizeof(s.topic));
    getStr("units", s.units, sizeof(s.units));
    s.homeOffsetDeg = getF("homeOff", s.homeOffsetDeg);
    s.backlashDeg   = getF("backlash", s.backlashDeg);
    s.trimLowDeg    = getF("trim0", 0);
    s.trimHighDeg   = getF("trimF", 0);
  }
  snprintf(k, sizeof(k), "topic%d", t); getStr(k, s.topic, sizeof(s.topic));
  snprintf(k, sizeof(k), "units%d", t); getStr(k, s.units, sizeof(s.units));
  snprintf(k, sizeof(k), "home%d", t);  s.homeOffsetDeg = getF(k, s.homeOffsetDeg);
  snprintf(k, sizeof(k), "back%d", t);  s.backlashDeg   = getF(k, s.backlashDeg);
  snprintf(k, sizeof(k), "tlo%d", t);   s.trimLowDeg    = getF(k, s.trimLowDeg);
  snprintf(k, sizeof(k), "thi%d", t);   s.trimHighDeg   = getF(k, s.trimHighDeg);
  prefs.end();
  if (!unitAllowed(t, s.units)) strlcpy(s.units, p.defaultUnits, sizeof(s.units));
}

void saveTypeSettings(int t, const TypeSettings& s) {
  char k[12];
  prefs.begin("ogauge", false);
  snprintf(k, sizeof(k), "topic%d", t); prefs.putString(k, s.topic);
  snprintf(k, sizeof(k), "units%d", t); prefs.putString(k, s.units);
  snprintf(k, sizeof(k), "home%d", t);  prefs.putFloat(k, s.homeOffsetDeg);
  snprintf(k, sizeof(k), "back%d", t);  prefs.putFloat(k, s.backlashDeg);
  snprintf(k, sizeof(k), "tlo%d", t);   prefs.putFloat(k, s.trimLowDeg);
  snprintf(k, sizeof(k), "thi%d", t);   prefs.putFloat(k, s.trimHighDeg);
  prefs.end();
}

void loadConfig() {
  prefs.begin("ogauge", true);
  cfg.gaugeType = getI("gtype", G_NONE);
  if (cfg.gaugeType < G_NONE || cfg.gaugeType >= G_COUNT) cfg.gaugeType = G_NONE;
  getStr("server", cfg.mqttServer, sizeof(cfg.mqttServer));
  cfg.mqttPort   = getI("port", cfg.mqttPort);
  getStr("user", cfg.mqttUser, sizeof(cfg.mqttUser));
  getStr("pass", cfg.mqttPass, sizeof(cfg.mqttPass));
  cfg.lightsMode = getI("lights", cfg.lightsMode);
  cfg.offHour    = getI("offH", cfg.offHour);
  cfg.offMinute  = getI("offM", cfg.offMinute);
  cfg.latitude   = getF("lat", cfg.latitude);
  cfg.longitude  = getF("lon", cfg.longitude);
  getStr("tz", cfg.tz, sizeof(cfg.tz));
  cfg.brightness = getI("bright", cfg.brightness);
  getStr("otaPass", cfg.otaPass, sizeof(cfg.otaPass));
  prefs.end();
  if (cfg.gaugeType >= 0) loadTypeSettings(cfg.gaugeType, ts);
}

void saveConfig() {
  prefs.begin("ogauge", false);
  prefs.putInt("gtype",     cfg.gaugeType);
  prefs.putString("server", cfg.mqttServer);
  prefs.putInt("port",      cfg.mqttPort);
  prefs.putString("user",   cfg.mqttUser);
  prefs.putString("pass",   cfg.mqttPass);
  prefs.putInt("lights",    cfg.lightsMode);
  prefs.putInt("offH",      cfg.offHour);
  prefs.putInt("offM",      cfg.offMinute);
  prefs.putFloat("lat",     cfg.latitude);
  prefs.putFloat("lon",     cfg.longitude);
  prefs.putString("tz",     cfg.tz);
  prefs.putInt("bright",    cfg.brightness);
  prefs.putString("otaPass", cfg.otaPass);
  prefs.end();
}

// ===========================================================================
// OBJECTS & STATE
// ===========================================================================
AccelStepper stepper(AccelStepper::HALF4WIRE, IN1_PIN, IN3_PIN, IN2_PIN, IN4_PIN);
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
WiFiClient   espClient;
PubSubClient client(espClient);
WebServer    web(80);

String deviceId;            // last 4 hex digits of the chip ID
float currentVal = NAN;     // latest reading (smoothed for pressure / temperature)
float shownVal   = NAN;     // value the pointer was last sent to
bool  coilsOn    = true;
bool  backlightOn = true;
bool  ledsDirty   = true;
bool  homed       = false;
unsigned long lastRehomeMs = 0;
bool  rehomeRequested = false;

// Feed check, shown on the settings page
unsigned long lastMsgMs = 0;      // 0 = nothing yet
bool  lastMsgHadValue = false;
unsigned long msgCount = 0;

// ===========================================================================
// HELPERS
// ===========================================================================
bool atMagnet() { return digitalRead(HALL_PIN) == LOW; }   // KY-003 pulls LOW on magnet

void hallPower(bool on) {
  if (on) {
    digitalWrite(HALL_PWR, HIGH);
    pinMode(HALL_PIN, INPUT_PULLUP);
    delay(20);
  } else {
    pinMode(HALL_PIN, INPUT);              // no leak back through the module's pull-up
    digitalWrite(HALL_PWR, LOW);
  }
}

String hostName() {
  String h = "opengauge-";
  if (cfg.gaugeType >= 0) h += String(P().name) + "-";
  h += deviceId;
  h.toLowerCase();
  return h;
}

float normDeg(float d) { d = fmodf(d, 360.0f); return d < 0 ? d + 360.0f : d; }
float angleDiff(float a, float b) { float d = normDeg(a - b); return d > 180.0f ? d - 360.0f : d; }

float minVal() { return P().val[0]; }
float maxVal() { return P().val[P().nPoints - 1]; }

// Bearing for a value on a partial dial, from the breakpoints (no trims)
float valueToDegRaw(float v) {
  const Profile& p = P();
  v = constrain(v, p.val[0], p.val[p.nPoints - 1]);
  for (int i = 1; i < p.nPoints; i++)
    if (v <= p.val[i]) {
      float f = (v - p.val[i - 1]) / (p.val[i] - p.val[i - 1]);
      return p.deg[i - 1] + f * (p.deg[i] - p.deg[i - 1]);
    }
  return p.deg[p.nPoints - 1];
}

// With the end trims: full at each end, none at the top (that's the home offset's job)
float valueToDeg(float v) {
  if (P().full360) return normDeg(v);
  float d = valueToDegRaw(v);
  if (d < 0) return d + ts.trimLowDeg  * (d / P().deg[0]);
  return d + ts.trimHighDeg * (d / P().deg[P().nPoints - 1]);
}

// Value at a bearing (partial dials; used to pick test points)
float degToValue(float d) {
  const Profile& p = P();
  if (p.full360) return normDeg(d);
  d = constrain(d, p.deg[0], p.deg[p.nPoints - 1]);
  for (int i = 1; i < p.nPoints; i++)
    if (d <= p.deg[i]) {
      float f = (d - p.deg[i - 1]) / (p.deg[i] - p.deg[i - 1]);
      return p.val[i - 1] + f * (p.val[i] - p.val[i - 1]);
    }
  return p.val[p.nPoints - 1];
}

float topValue() { return degToValue(0); }

long valueToSteps(float v) {
  if (!P().full360) return lround(valueToDeg(v) * stepsPerDegree);    // 0 = 12 o'clock
  // Whole circle: shortest way round from where we are
  long target = lround(normDeg(v) * stepsPerDegree);
  long cur    = stepper.currentPosition();
  long curMod = ((cur % stepsPerRevolution) + stepsPerRevolution) % stepsPerRevolution;
  long delta  = target - curMod;
  if (delta >  stepsPerRevolution / 2) delta -= stepsPerRevolution;
  if (delta < -stepsPerRevolution / 2) delta += stepsPerRevolution;
  return cur + delta;
}

void applyHomeOffset() {
  stepper.setCurrentPosition(-lround(ts.homeOffsetDeg * stepsPerDegree));
}

// Moves always finish travelling clockwise (takes up gear backlash)
long finalTarget = 0;
bool finalPending = false;

void moveToSteps(long target) {
  if (target < stepper.currentPosition()) {
    stepper.moveTo(target - lround(ts.backlashDeg * stepsPerDegree));
    finalTarget = target;
    finalPending = true;
  } else {
    stepper.moveTo(target);
    finalPending = false;
  }
}

bool continueMove() {
  if (!finalPending) return false;
  finalPending = false;
  stepper.moveTo(finalTarget);
  return true;
}

void runToPosition(long pos) {
  moveToSteps(pos);
  do {
    while (stepper.distanceToGo() != 0) stepper.run();
  } while (continueMove());
}

void powerCoils() {
  if (!coilsOn) { stepper.enableOutputs(); coilsOn = true; }
}

// ===========================================================================
// TREND (pressure, temperature): 3-hour tendency, worked out on the board
// ===========================================================================
const unsigned long slotMs = 10UL * 60UL * 1000UL;   // one average per 10 min
const int SLOTS = 18;                                // = 3 hours
float history[SLOTS];
int   histCount = 0, histHead = 0;
float slotSum = 0;  uint16_t slotN = 0;
unsigned long slotStartMs = 0;
enum Trend { TREND_STEADY, TREND_RISING, TREND_FALLING };
Trend trend = TREND_STEADY;
bool  trendFast = false;
float trendChange3h = 0;
const int minHistoryMins = 60;

void saveHistory() {
  time_t now = time(nullptr);
  if (now < 1700000000) return;
  prefs.begin("trend", false);
  prefs.putInt("type", cfg.gaugeType);
  prefs.putBytes("hist", history, sizeof(history));
  prefs.putInt("count", histCount);
  prefs.putInt("head", histHead);
  prefs.putULong("ts", (uint32_t)now);
  prefs.end();
}

void loadHistory() {
  time_t now = time(nullptr);
  prefs.begin("trend", true);
  bool ok = prefs.isKey("ts") && getI("type", -1) == cfg.gaugeType;
  uint32_t t = ok ? prefs.getULong("ts", 0) : 0;
  if (ok && now > 1700000000 && t && now - t < 20 * 60) {   // rebooted within 20 minutes
    prefs.getBytes("hist", history, sizeof(history));
    histCount = prefs.getInt("count", 0);
    histHead  = prefs.getInt("head", 0);
    Serial.printf("Restored %d trend samples (%d min).\n", histCount, histCount * 10);
  } else {
    Serial.println("Building the trend history from scratch.");
  }
  prefs.end();
}

void computeTrend() {
  const Profile& p = P();
  Trend old = trend; bool oldFast = trendFast;
  int spanMins = histCount * 10;
  if (isnan(currentVal) || spanMins < minHistoryMins) {
    trend = TREND_STEADY; trendFast = false;
  } else {
    float oldest = history[(histHead - histCount + SLOTS) % SLOTS];
    trendChange3h = (currentVal - oldest) * (180.0f / spanMins);
    float mag = fabsf(trendChange3h);
    if (trend == TREND_STEADY) {
      if (mag >= p.trendOn) trend = trendChange3h > 0 ? TREND_RISING : TREND_FALLING;
    } else {
      if (mag < p.trendOff) trend = TREND_STEADY;
      else trend = trendChange3h > 0 ? TREND_RISING : TREND_FALLING;
    }
    trendFast = (trend != TREND_STEADY) && mag >= p.trendFast;
  }
  if (trend != old || trendFast != oldFast) {
    ledsDirty = true;
    Serial.printf("Trend: %s%s (%.2f %s / 3 h)\n",
                  trend == TREND_RISING ? "RISING" : trend == TREND_FALLING ? "FALLING" : "STEADY",
                  trendFast ? " fast" : "", trendChange3h, p.unit);
  }
}

void updateHistorySlot() {
  if (millis() - slotStartMs < slotMs) return;
  slotStartMs += slotMs;
  if (slotN > 0) {
    history[histHead] = slotSum / slotN;
    histHead = (histHead + 1) % SLOTS;
    if (histCount < SLOTS) histCount++;
    saveHistory();
  }
  slotSum = 0; slotN = 0;
  computeTrend();
}

// ===========================================================================
// DAY'S MAXIMUM (wind speed): LED beside today's highest gust
// ===========================================================================
float dayMax = NAN;
long  maxDay = -1;

long todayKey() {
  struct tm t;
  if (!getLocalTime(&t, 10)) return -1;
  return (t.tm_year + 1900) * 10000L + (t.tm_mon + 1) * 100L + t.tm_mday;
}

void checkNewDay() {
  long d = todayKey();
  if (d > 0 && d != maxDay) {
    maxDay = d; dayMax = NAN;
    prefs.begin("daymax", false);
    prefs.putLong("day", maxDay);
    prefs.putFloat("max", NAN);
    prefs.end();
    ledsDirty = true;
  }
}

int valueToLed(float v);

void updateDayMax(float v) {
  checkNewDay();
  if (maxDay < 0) return;
  if (isnan(dayMax) || v > dayMax) {
    if (isnan(dayMax) || valueToLed(v) != valueToLed(dayMax)) ledsDirty = true;
    dayMax = v;
    prefs.begin("daymax", false);
    prefs.putFloat("max", dayMax);
    prefs.end();
  }
}

void loadDayMax() {
  long d = todayKey();
  prefs.begin("daymax", true);
  if (d > 0 && prefs.isKey("day") && prefs.getLong("day", -1) == d) {
    maxDay = d;
    dayMax = prefs.getFloat("max", NAN);
  }
  prefs.end();
}

// ===========================================================================
// LEDS
// ===========================================================================
int markerLed = -1;                 // follows the pointer in the wind speed self-test
int finderLed = -1;                 // LED finder on the settings page
unsigned long finderUntil = 0;

int valueToLed(float v) {
  float bearing = valueToDeg(v);
  float d = fmodf((bearing - led0BearingDeg) * ledDirection, 360.0f);
  if (d < 0) d += 360.0f;
  int i = lround(d / degPerLed);
  int perTurn = lround(360.0f / degPerLed);
  return i >= perTurn ? i - perTurn : i;
}

void setRGB(int i, const uint8_t rgb[3], float k) {
  if (i < 0 || i >= NUM_LEDS) return;
  strip.setPixelColor(i, strip.Color(rgb[0] * k, rgb[1] * k, rgb[2] * k));
}

void updateLeds() {
  const Profile& p = P();
  const float backK = backlightOn ? constrain(cfg.brightness, 0, 255) / 255.0f : 0.0f;
  for (int i = 0; i < NUM_LEDS; i++) setRGB(i, backlightRGB, backK);

  if (p.extra == EX_TREND && trend != TREND_STEADY) {
    float level = HIGHLIGHT_BRIGHT;
    if (trendFast) {
      float wave = 0.5f + 0.5f * sinf(millis() / 1000.0f * TWO_PI / pulsePeriodS);
      level = TREND_FADE_MIN + (HIGHLIGHT_BRIGHT - TREND_FADE_MIN) * wave;
    }
    if (trend == TREND_RISING)  for (int i = 0; i < p.nRising;  i++) setRGB(p.risingLeds[i],  p.risingRGB,  level / 255.0f);
    if (trend == TREND_FALLING) for (int i = 0; i < p.nFalling; i++) setRGB(p.fallingLeds[i], p.fallingRGB, level / 255.0f);
  }
  const uint8_t red[3] = {255, 0, 0};
  int m = markerLed >= 0 ? markerLed : (p.extra == EX_DAYMAX && !isnan(dayMax) ? valueToLed(dayMax) : -1);
  if (m >= 0) setRGB(m, red, HIGHLIGHT_BRIGHT / 255.0f);
  if (!homed) strip.setPixelColor(WARN_LED, strip.Color(60, 0, 0));
  if (finderLed >= 0) setRGB(finderLed, red, 1.0f);
  strip.show();
}

void showStatus(uint8_t r, uint8_t g, uint8_t b) { strip.fill(strip.Color(r, g, b)); strip.show(); }
#define STATUS_STARTED   showStatus(80, 0, 0)
#define STATUS_SETUP     showStatus(0, 70, 70)
#define STATUS_WIFI      showStatus(0, 0, 80)
#define STATUS_HOMING    showStatus(80, 60, 0)
#define STATUS_SELFTEST  showStatus(60, 0, 80)

void flashRed(int times) {
  for (int i = 0; i < times; i++) { showStatus(120, 0, 0); delay(250); showStatus(0, 0, 0); delay(250); }
}

// ===========================================================================
// HOMING — never hangs: gives up after two turns
// ===========================================================================
bool stepToMagnet(long maxSteps) {
  long limit = stepper.currentPosition() + maxSteps;
  while (atMagnet()  && stepper.currentPosition() < limit) stepper.runSpeed();   // step off
  while (!atMagnet() && stepper.currentPosition() < limit) stepper.runSpeed();   // roll on
  return stepper.currentPosition() < limit;
}

bool seekMagnetAndZero() {
  hallPower(true);
  powerCoils();
  stepper.setMaxSpeed(homingSpeed);
  stepper.setSpeed(homingSpeed);
  Serial.printf("Homing... hall sensor reads %s\n", atMagnet() ? "MAGNET" : "no magnet");
  bool ok = stepToMagnet(2L * stepsPerRevolution);
  applyHomeOffset();
  if (!ok) Serial.println("Homing FAILED: magnet not found in 2 turns (flip the magnet? check the KY-003 wiring)");
  return ok;
}

void calibrateStepsPerRevolution() {
  stepper.setCurrentPosition(0);
  stepper.setSpeed(homingSpeed);
  bool ok = stepToMagnet(2L * 4096);
  long measured = labs(stepper.currentPosition());
  if (ok && measured > 100) {
    stepsPerRevolution = measured;
    stepsPerDegree = stepsPerRevolution / 360.0;
  } else {
    Serial.println("Calibration looked wrong; keeping the default.");
  }
  applyHomeOffset();
  Serial.printf("Steps/rev: %ld  steps/deg: %.4f\n", stepsPerRevolution, stepsPerDegree);
}

void homeGauge(bool calibrate, bool retry) {
  bool ok = seekMagnetAndZero();
  if (!ok && retry) { flashRed(3); Serial.println("Retrying homing..."); ok = seekMagnetAndZero(); }
  if (ok && calibrate) { delay(1000); calibrateStepsPerRevolution(); }
  hallPower(false);
  if (!ok) {
    flashRed(3);
    stepper.setCurrentPosition(0);         // best guess: treat here as 12 o'clock
    Serial.println("Carrying on without homing; will retry in an hour.");
  }
  homed = ok;
  stepper.setMaxSpeed(runSpeed);
  stepper.setAcceleration(runAccel);
  runToPosition(0);                        // park at 12 o'clock
}

void rehome() {
  Serial.println("Re-homing...");
  homeGauge(false, false);
  shownVal = NAN;
  ledsDirty = true;
}

// ===========================================================================
// READING THE FEED
// ===========================================================================
float readNumber(JsonVariant v) {
  if (v.is<const char*>()) return atof(v.as<const char*>());
  if (v.is<float>() || v.is<long>()) return v.as<float>();
  return NAN;
}

// Convert to the gauge's own units (mm, °C, mph, hPa, degrees)
float toGaugeUnits(float v, const char* u) {
  if (!strcmp(u, "cm"))   return v * 10.0f;
  if (!strcmp(u, "in"))   return v * 25.4f;
  if (!strcmp(u, "F"))    return (v - 32.0f) * 5.0f / 9.0f;
  if (!strcmp(u, "kph"))  return v * 0.621371f;
  if (!strcmp(u, "mps"))  return v * 2.236936f;
  if (!strcmp(u, "knot")) return v * 1.150779f;
  if (!strcmp(u, "inHg")) return v * 33.8639f;
  return v;
}

bool endsWith(const char* s, const char* suffix) {
  size_t ls = strlen(s), lf = strlen(suffix);
  return ls >= lf && strcmp(s + ls - lf, suffix) == 0;
}

float extractValue(const char* topic, byte* payload, unsigned int length) {
  const Profile& p = P();
  unsigned int i = 0;
  while (i < length && isspace(payload[i])) i++;
  if (i >= length) return NAN;
  float v = NAN;

  if (payload[i] != '{') {                                 // plain number
    char buf[24]; unsigned int n = min(length - i, (unsigned int)sizeof(buf) - 1);
    memcpy(buf, payload + i, n); buf[n] = 0;
    if (!isdigit(buf[0]) && buf[0] != '-' && buf[0] != '.') return NAN;   // "None", "null"
    const char* u = p.plainUnits;
    for (int s = 0; s < p.nSuffixes; s++)
      if (endsWith(topic, p.suffixes[s])) u = p.suffixes[s] + 1;
    v = toGaugeUnits(atof(buf), u);
  } else {                                                 // weeWX JSON loop packet
    JsonDocument doc;
    if (deserializeJson(doc, (const char*)payload, length)) return NAN;
    for (int k = 0; k < p.nKeys && isnan(v); k++) {
      JsonVariant jv = doc[p.keys[k].key];
      if (jv.isNull()) continue;                           // e.g. calm: no wind direction
      float raw = readNumber(jv);
      if (isnan(raw)) continue;
      v = toGaugeUnits(raw, p.keys[k].unit ? p.keys[k].unit : ts.units);
    }
  }
  if (cfg.gaugeType == G_PRESSURE && !isnan(v) && v < 40) v *= 33.8639f;   // inHg spotted
  return v;
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  const Profile& p = P();
  lastMsgMs = millis(); msgCount++;
  float v = extractValue(topic, payload, length);
  lastMsgHadValue = !isnan(v) && v >= p.minValid && v <= p.maxValid;
  if (!lastMsgHadValue) return;
  if (p.full360) v = normDeg(v);

  currentVal = (isnan(currentVal) || p.smoothing >= 1.0f) ? v : currentVal + p.smoothing * (v - currentVal);
  if (p.extra == EX_TREND)  { slotSum += v; slotN++; computeTrend(); }
  if (p.extra == EX_DAYMAX) updateDayMax(v);

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 60000) {
    lastPrint = millis();
    Serial.printf("%s %.1f %s\n", p.title, currentVal, p.unit);
  }
}

void connectMqtt() {
  client.disconnect();
  client.setServer(cfg.mqttServer, cfg.mqttPort);
}

void serviceMqtt() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (!client.connected()) {
    static unsigned long lastTry = 0;
    if (lastTry && millis() - lastTry < 5000) return;     // retry every 5 s, never block
    lastTry = millis();
    String id = "OpenGauge-" + String(P().name) + "-" + deviceId;
    Serial.print("Connecting to MQTT... ");
    bool ok = strlen(cfg.mqttUser) ? client.connect(id.c_str(), cfg.mqttUser, cfg.mqttPass)
                                   : client.connect(id.c_str());
    if (ok) { Serial.printf("connected, topic %s\n", ts.topic); client.subscribe(ts.topic); }
    else    Serial.printf("failed rc=%d\n", client.state());
    return;
  }
  client.loop();
}

// ===========================================================================
// SETUP PAGE (first start, or BOOT held at power-up)
// ===========================================================================
String apName() {
  return cfg.gaugeType >= 0 ? "OpenGauge-" + String(P().name) + "-" + deviceId : "OpenGauge-" + deviceId;
}

String unitOptions(int t, const char* selected) {
  String out, list = profiles[t].unitChoices;
  int start = 0;
  while (start < (int)list.length()) {
    int comma = list.indexOf(',', start); if (comma < 0) comma = list.length();
    String item = list.substring(start, comma);
    int colon = item.indexOf(':');
    String v = item.substring(0, colon), l = item.substring(colon + 1);
    out += "<option value='" + v + "'" + (v == selected ? " selected" : "") + ">" + l + "</option>";
    start = comma + 1;
  }
  return out;
}

bool runSetupPortal(unsigned long timeoutS, bool onlyIfNoWiFi, bool showWiFiPassword) {
  WiFiManager wm;                          // fresh each time, so parameters never pile up
  wm.setHostname(hostName().c_str());
  wm.setShowPassword(showWiFiPassword);    // someone's at the gauge: no need to retype it
  // Each type's own topic and units, so changing the type fills in the right ones
  TypeSettings all[G_COUNT];
  for (int t = 0; t < G_COUNT; t++) loadTypeSettings(t, all[t]);
  int curType = cfg.gaugeType;

  String typeHtml = "<h3>Gauge</h3><label for='gtype'>Which gauge is this?</label><select name='gtype' id='gtype'>";
  if (curType < 0) typeHtml += "<option value='-1' selected>— choose —</option>";
  for (int t = 0; t < G_COUNT; t++)
    typeHtml += "<option value='" + String(t) + "'" + (t == curType ? " selected" : "") + ">" + profiles[t].title + "</option>";
  typeHtml += "</select>";

  int ut = curType < 0 ? G_RAIN : curType;
  String unitsHtml = "<label for='units'>Units of a key with no units in its name</label><select name='units' id='units'>" +
                     unitOptions(ut, all[ut].units) + "</select>";

  String js = "<script>var T=[";
  for (int t = 0; t < G_COUNT; t++) {
    String tp = all[t].topic; tp.replace("\\", "\\\\"); tp.replace("\"", "\\\"");
    String uo = unitOptions(t, all[t].units); uo.replace("\"", "\\\"");
    js += String(t ? "," : "") + "{t:\"" + tp + "\",u:\"" + uo + "\"}";
  }
  js += "];document.getElementById('gtype').onchange=function(){var d=T[this.value];if(!d)return;"
        "document.getElementById('topic').value=d.t;document.getElementById('units').innerHTML=d.u;};</script>";

  char portBuf[8], latBuf[16], lonBuf[16];
  snprintf(portBuf, sizeof(portBuf), "%d", cfg.mqttPort);
  snprintf(latBuf, sizeof(latBuf), "%.4f", cfg.latitude);
  snprintf(lonBuf, sizeof(lonBuf), "%.4f", cfg.longitude);

  WiFiManagerParameter pType(typeHtml.c_str());
  WiFiManagerParameter hFeed("<h3>Weather feed (MQTT)</h3><p><small>Filled in with a live demo station, "
                             "so the gauge works straight away. Change it to your own feed.</small></p>");
  WiFiManagerParameter pServer("server", "MQTT server", cfg.mqttServer, sizeof(cfg.mqttServer) - 1);
  WiFiManagerParameter pPort("port", "Port", portBuf, 6, "type='number'");
  WiFiManagerParameter pUser("user", "Username (blank if none)", cfg.mqttUser, sizeof(cfg.mqttUser) - 1);
  WiFiManagerParameter pPass("pass", "Password (blank if none)", cfg.mqttPass, sizeof(cfg.mqttPass) - 1, "type='password' autocomplete='off'");
  WiFiManagerParameter pTopic("topic", "Topic", all[ut].topic, sizeof(ts.topic) - 1);
  WiFiManagerParameter pUnits(unitsHtml.c_str());
  WiFiManagerParameter pJs(js.c_str());
  WiFiManagerParameter hLoc("<h3>Location</h3><p><small>For switching the backlight on at dusk. Decimal degrees, "
                            "west negative (London 51.51, -0.13). In Google Maps, long-press a spot to see them. "
                            "Backlight mode and pointer tuning are on the settings page once the gauge is running.</small></p>");
  WiFiManagerParameter pLat("lat", "Latitude", latBuf, 12, "type='number' step='any'");
  WiFiManagerParameter pLon("lon", "Longitude", lonBuf, 12, "type='number' step='any'");
  WiFiManagerParameter pOta("ota", "Password for wireless uploads (blank = none)", cfg.otaPass, sizeof(cfg.otaPass) - 1,
                            "type='password' autocomplete='new-password'");

  WiFiManagerParameter* params[] = { &pType, &hFeed, &pServer, &pPort, &pUser, &pPass, &pTopic, &pUnits, &pJs,
                                     &hLoc, &pLat, &pLon, &pOta };
  for (auto* p : params) wm.addParameter(p);

  wm.setSaveParamsCallback([&]() {
    if (wm.server->hasArg("gtype")) {
      int t = wm.server->arg("gtype").toInt();
      if (t >= 0 && t < G_COUNT) cfg.gaugeType = t;
    }
    strlcpy(cfg.mqttServer, pServer.getValue(), sizeof(cfg.mqttServer));
    cfg.mqttPort = atoi(pPort.getValue()); if (cfg.mqttPort <= 0) cfg.mqttPort = 1883;
    strlcpy(cfg.mqttUser, pUser.getValue(), sizeof(cfg.mqttUser));
    strlcpy(cfg.mqttPass, pPass.getValue(), sizeof(cfg.mqttPass));
    cfg.latitude  = constrain(atof(pLat.getValue()), -90.0, 90.0);
    cfg.longitude = constrain(atof(pLon.getValue()), -180.0, 180.0);
    strlcpy(cfg.otaPass, pOta.getValue(), sizeof(cfg.otaPass));
    saveConfig();
    if (cfg.gaugeType >= 0) {
      TypeSettings s = all[cfg.gaugeType];
      strlcpy(s.topic, pTopic.getValue(), sizeof(s.topic));
      if (wm.server->hasArg("units")) strlcpy(s.units, wm.server->arg("units").c_str(), sizeof(s.units));
      if (!unitAllowed(cfg.gaugeType, s.units)) strlcpy(s.units, profiles[cfg.gaugeType].defaultUnits, sizeof(s.units));
      saveTypeSettings(cfg.gaugeType, s);
      ts = s;
    }
    Serial.println("Settings saved.");
  });

  wm.setTitle("Open Gauges");
  wm.setBreakAfterConfig(true);            // come back after Save even if the Wi-Fi fails
  wm.setConfigPortalTimeout(timeoutS);
  wm.setConnectTimeout(20);
  wm.setConnectRetries(3);
  wm.setAPCallback([](WiFiManager*) {
    STATUS_SETUP;
    Serial.printf("Setup page: join Wi-Fi \"%s\" (password %s), then browse to 192.168.4.1\n",
                  apName().c_str(), SETUP_AP_PASSWORD);
  });

  String ap = apName();
  bool connected = onlyIfNoWiFi ? wm.autoConnect(ap.c_str(), SETUP_AP_PASSWORD)
                                : wm.startConfigPortal(ap.c_str(), SETUP_AP_PASSWORD);
  return connected;
}

bool setupButtonHeld() {                   // BOOT held for 0.7 s in the first 2 s
  pinMode(SETUP_BTN_PIN, INPUT_PULLUP);
  unsigned long start = millis(), heldSince = 0;
  while (millis() - start < 2000) {
    if (digitalRead(SETUP_BTN_PIN) == LOW) {
      if (!heldSince) heldSince = millis();
      if (millis() - heldSince > 700) return true;
    } else heldSince = 0;
    delay(10);
  }
  return false;
}

void connectWiFi(bool forceSetup) {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  bool saved;
  String savedSsid;
  { WiFiManager probe; saved = probe.getWiFiIsSaved(); savedSsid = probe.getWiFiSSID(); }

  bool ok;
  if (forceSetup || !saved || cfg.gaugeType < 0) {
    Serial.println(forceSetup ? "BOOT held: opening the setup page" : "Opening the setup page");
    ok = runSetupPortal(0, false, forceSetup);
    while (cfg.gaugeType < 0) {            // can't run without knowing which gauge it is
      Serial.println("No gauge type chosen yet");
      flashRed(2);
      ok = runSetupPortal(0, false, false);
    }
  } else {
    STATUS_WIFI;
    Serial.printf("Connecting to Wi-Fi %s...\n", savedSsid.c_str());
    ok = runSetupPortal(180, true, false);        // setup page for 3 min only if the Wi-Fi fails
  }

  STATUS_WIFI;
  if (!ok && WiFi.status() != WL_CONNECTED) { WiFi.mode(WIFI_STA); WiFi.begin(); }
  for (int t = 1; !ok && WiFi.status() != WL_CONNECTED; t++) {     // keep trying, never reboot
    delay(500); Serial.print(".");
    if (t % 40 == 0) { Serial.println("\nStill trying Wi-Fi..."); WiFi.disconnect(); WiFi.begin(); }
  }
  Serial.printf("\nWi-Fi connected, IP %s\n", WiFi.localIP().toString().c_str());
}

// ===========================================================================
// WIRELESS UPDATES
// ===========================================================================
void setupOTA() {
  ArduinoOTA.setHostname(hostName().c_str());
  if (strlen(cfg.otaPass)) ArduinoOTA.setPassword(cfg.otaPass);
  ArduinoOTA.onStart([]() {
    stepper.stop(); stepper.disableOutputs(); coilsOn = false;
    client.disconnect();
    showStatus(0, 0, 0);
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    int lit = (int)((uint64_t)done * NUM_LEDS / total);
    for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, i < lit ? strip.Color(0, 80, 0) : 0);
    strip.show();
  });
  ArduinoOTA.onError([](ota_error_t e) { Serial.printf("OTA error %u\n", e); flashRed(3); ledsDirty = true; });
  ArduinoOTA.begin();
}

// ===========================================================================
// BACKLIGHT SCHEDULE
// ===========================================================================
int duskMinutesLocal(const struct tm& local, const struct tm& utc) {
  const float D = PI / 180.0f;
  int   N       = local.tm_yday + 1;
  float lngHour = cfg.longitude / 15.0f;
  float t = N + ((18.0f - lngHour) / 24.0f);
  float M = 0.9856f * t - 3.289f;
  float L = fmodf(M + 1.916f * sinf(M * D) + 0.020f * sinf(2 * M * D) + 282.634f + 360.0f, 360.0f);
  float RA = fmodf(atanf(0.91764f * tanf(L * D)) / D + 360.0f, 360.0f);
  RA += floorf(L / 90.0f) * 90.0f - floorf(RA / 90.0f) * 90.0f;
  RA /= 15.0f;
  float sinDec = 0.39782f * sinf(L * D);
  float cosDec = cosf(asinf(sinDec));
  float zenith = 90.0f - duskSunAngle;
  float cosH = (cosf(zenith * D) - sinDec * sinf(cfg.latitude * D)) / (cosDec * cosf(cfg.latitude * D));
  if (cosH < -1) return -1;
  if (cosH >  1) return 0;
  float H  = acosf(cosH) / D / 15.0f;
  float UT = fmodf(H + RA - 0.06571f * t - 6.622f - lngHour + 48.0f, 24.0f);
  int offset = (local.tm_hour * 60 + local.tm_min) - (utc.tm_hour * 60 + utc.tm_min);
  if (offset >  720) offset -= 1440;
  if (offset < -720) offset += 1440;
  return ((int)lroundf(UT * 60.0f) + offset + 1440) % 1440;
}

int duskToday() {                          // -1 = clock not set yet, or no dusk today
  time_t now = time(nullptr);
  if (now < 1700000000) return -1;
  struct tm local, utc;
  localtime_r(&now, &local);
  gmtime_r(&now, &utc);
  return duskMinutesLocal(local, utc);
}

void updateBacklightState() {
  bool on = true;
  if (cfg.lightsMode == LIGHTS_OFF) on = false;
  else if (cfg.lightsMode == LIGHTS_DUSK) {
    time_t now = time(nullptr);
    if (now > 1700000000) {                // until the clock is set, stay on
      struct tm local;
      localtime_r(&now, &local);
      int nowMin  = local.tm_hour * 60 + local.tm_min;
      int offMin  = cfg.offHour * 60 + cfg.offMinute;
      int duskMin = duskToday();
      if (duskMin < 0)            on = false;
      else if (offMin <= duskMin) on = (nowMin >= duskMin) || (nowMin < offMin);
      else                        on = (nowMin >= duskMin) && (nowMin < offMin);
    }
  }
  if (on != backlightOn) {
    backlightOn = on;
    ledsDirty = true;
    Serial.printf("Backlight %s\n", on ? "on" : "off");
  }
}

// ===========================================================================
// SETTINGS PAGE — http://opengauge-<type>-xxxx.local
// ===========================================================================
const unsigned long webOpenMs = 10UL * 60UL * 1000UL;
bool webOn = false;
unsigned long webOffAt = 0;

bool  testMode = false;              // pointer follows the settings page, not the feed
unsigned long testSinceMs = 0;
const unsigned long testTimeoutMs = 10UL * 60UL * 1000UL;
float testQueue[4];
int   testCount = 0;
unsigned long testDwellUntil = 0;
bool  tuneDirty = false;

void openWeb() {
  if (!webOn) { web.begin(); webOn = true; Serial.printf("Settings page: http://%s.local\n", hostName().c_str()); }
  webOffAt = millis() + webOpenMs;
}

void queueTest(std::initializer_list<float> values) {
  testCount = 0;
  for (float v : values) if (testCount < 4) testQueue[testCount++] = v;
  testMode = true; testSinceMs = millis(); testDwellUntil = 0;
}

void endTest() { testMode = false; testCount = 0; shownVal = NAN; }

void sendJson(JsonDocument& doc) {
  webOffAt = millis() + webOpenMs;         // in use: keep the page open
  String out;
  serializeJson(doc, out);
  web.send(200, "application/json; charset=utf-8", out);
}

void sendInfo() {
  const Profile& p = P();
  JsonDocument d;
  d["type"] = cfg.gaugeType;
  d["title"] = p.title;
  d["unit"] = p.unit;
  d["full360"] = p.full360;
  d["host"] = hostName();
  JsonArray types = d["types"].to<JsonArray>();
  for (int t = 0; t < G_COUNT; t++) types.add(profiles[t].title);
  JsonArray marks = d["marks"].to<JsonArray>();
  if (p.full360) { marks.add(0); marks.add(90); marks.add(180); marks.add(270); }
  else if (p.nPoints == 2) { marks.add(minVal()); marks.add(topValue()); marks.add(maxVal()); }
  else for (int i = 0; i < p.nPoints; i++) marks.add(p.val[i]);
  d["units"] = unitOptions(cfg.gaugeType, ts.units);
  sendJson(d);
}

void sendStatus() {
  const Profile& p = P();
  JsonDocument d;
  if (isnan(currentVal)) d["v"] = nullptr; else d["v"] = currentVal;
  d["test"] = testMode;
  d["homed"] = homed;
  d["dirty"] = tuneDirty;
  d["home"] = ts.homeOffsetDeg; d["back"] = ts.backlashDeg;
  d["tlo"] = ts.trimLowDeg;     d["thi"] = ts.trimHighDeg;
  d["lm"] = cfg.lightsMode;
  char off[6]; snprintf(off, sizeof(off), "%02d:%02d", cfg.offHour, cfg.offMinute); d["off"] = off;
  d["lat"] = cfg.latitude; d["lon"] = cfg.longitude;
  d["bright"] = cfg.brightness;
  d["lit"] = backlightOn;
  d["dusk"] = duskToday();
  d["server"] = cfg.mqttServer; d["port"] = cfg.mqttPort; d["user"] = cfg.mqttUser;
  d["topic"] = ts.topic; d["units"] = ts.units;
  d["mqtt"] = client.connected();
  d["age"] = lastMsgMs ? (long)((millis() - lastMsgMs) / 1000) : -1;
  d["ok"] = lastMsgHadValue;
  d["msgs"] = msgCount;
  if (p.extra == EX_TREND)
    d["extra"] = histCount * 10 < minHistoryMins ? String("Trend: building history (") + histCount * 10 + " of 60 min)"
               : String("Trend: ") + (trend == TREND_RISING ? "rising" : trend == TREND_FALLING ? "falling" : "steady") +
                 (trendFast ? " fast" : "") + " (" + String(trendChange3h, 1) + " " + p.unit + " / 3 h)";
  if (p.extra == EX_DAYMAX)
    d["extra"] = isnan(dayMax) ? String("Today's max: none yet") : String("Today's max: ") + String(dayMax, 1) + " " + p.unit;
  sendJson(d);
}

const char SETTINGS_PAGE[] PROGMEM = R"HTML(<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Open Gauges</title>
<style>
body{font-family:-apple-system,system-ui,sans-serif;max-width:440px;margin:auto;padding:12px;background:#111;color:#eee}
h2{margin:.2em 0}h3{margin:1.4em 0 .4em;border-top:1px solid #333;padding-top:.8em}p{color:#aaa;font-size:.9em;margin:.35em 0}
button{font-size:1em;padding:.7em;margin:3px 0;border:0;border-radius:8px;background:#2a6;color:#fff;width:100%}
input,select{font-size:1.1em;width:100%;box-sizing:border-box;padding:.45em;margin:2px 0;border-radius:8px;border:1px solid #555;background:#222;color:#fff}
label{font-size:.85em;color:#aaa}.row{display:flex;gap:6px;align-items:center}.row>*{flex:1}
.alt{background:#456}.warn{background:#a52}.st{font-size:.9em;color:#8c8}.bad{color:#e96}
</style></head><body>
<h2 id="title">Open Gauges</h2><div class="st" id="st">…</div>

<h3>Backlight</h3>
<select id="lm"><option value="2">On at dusk</option><option value="0">On all the time</option><option value="1">Off all the time</option></select>
<label>Off at (for “On at dusk”; 00:00 is midnight)</label><input id="off" type="time">
<label>Location, for working out dusk: decimal degrees, west negative (London 51.51, −0.13). Long-press a spot in Google Maps to see them.</label>
<div class="row"><input id="lat" type="number" step="any" inputmode="decimal" placeholder="Latitude">
<input id="lon" type="number" step="any" inputmode="decimal" placeholder="Longitude"></div>
<label>Brightness (0–255)</label><input id="bright" type="number" min="0" max="255">
<button onclick="lights()">Apply &amp; save backlight</button>
<p id="lt"></p>

<h3>Weather feed</h3>
<p id="feedst"></p>
<label>MQTT server</label><input id="server">
<div class="row"><div><label>Port</label><input id="port" type="number"></div>
<div><label>Units of a bare key</label><select id="units"></select></div></div>
<div class="row"><div><label>Username</label><input id="user" autocomplete="off"></div>
<div><label>Password</label><input id="pass" type="password" placeholder="unchanged" autocomplete="new-password"></div></div>
<label>Topic</label><input id="topic">
<button onclick="feed()">Apply &amp; save feed</button>

<h3>Pointer tuning</h3>
<p><b>Home offset</b>: how far the pointer turns on from the magnet to reach the top of the dial (12 o'clock).
<b>Apply &amp; home</b> finds the magnet, then turns on by this much. Stops left of the top: increase. Goes past: decrease.</p>
<div class="row"><input id="home" type="number" step="0.1" inputmode="decimal">
<button onclick="setv('home')">Apply &amp; home</button></div>
<p><b>Backlash</b>: the gears have a little play. <b>Apply &amp; test</b> brings the pointer to the top from the left,
pauses, then from the right. If the second stop is to the right of the first, increase it until they match. Too much does no harm.</p>
<div class="row"><input id="back" type="number" step="0.5" min="0" inputmode="decimal">
<button onclick="setv('back')">Apply &amp; test</button></div>
<div id="ends"><p><b>Ends of the scale</b>: if the top is right but an end still sits a degree or two off its mark,
trim it here. Full at that end, fading to nothing at the top. Positive turns clockwise (right).</p>
<div class="row"><input id="tlo" type="number" step="0.5" inputmode="decimal">
<button onclick="setv('tlo')" id="tlob">Apply &amp; go to start</button></div>
<div class="row"><input id="thi" type="number" step="0.5" inputmode="decimal">
<button onclick="setv('thi')" id="thib">Apply &amp; go to end</button></div></div>
<p>Check the dial:</p><div class="row" id="marks"></div>
<button class="alt" onclick="go('/sweep')">Sweep the whole scale</button>
<button onclick="go('/save')">Save tuning</button>
<div class="row"><button class="alt" onclick="go('/live')">Back to live readings</button>
<button class="warn" onclick="go('/rehome')">Re-home now</button></div>
<p>While tuning, the gauge ignores the feed; it goes back to live readings by itself after 10 minutes.</p>

<h3>LED finder</h3>
<p>Lights one LED red for 5 seconds, to find the numbers beside the words on your dial.</p>
<div class="row"><input id="led" type="number" min="0" max="83" value="0"><button class="alt" onclick="go('/led?n='+led.value)">Light it</button></div>

<h3>Gauge type</h3>
<p>Each type keeps its own feed and tuning. Changing type restarts the gauge.</p>
<div class="row"><select id="gtype"></select><button class="warn" onclick="gtypeSet()">Change &amp; restart</button></div>
<p>This page closes 10 minutes after the gauge starts, or after you last use it. Restart the gauge or press its BOOT button to open it again.</p>

<script>
let I={},first=true;
const $=id=>document.getElementById(id);
const hm=m=>String(Math.floor(m/60)).padStart(2,'0')+':'+String(m%60).padStart(2,'0');
const q=o=>Object.keys(o).map(k=>k+'='+encodeURIComponent(o[k])).join('&');
function info(j){I=j;$('title').textContent='Open Gauges: '+j.title;$('units').innerHTML=j.units;
 $('gtype').innerHTML=j.types.map((t,i)=>'<option value="'+i+'"'+(i==j.type?' selected':'')+'>'+t+'</option>').join('');
 $('marks').innerHTML=j.marks.map(m=>'<button class="alt" onclick="go(\'/go?v='+m+'\')">'+m+(j.full360?'°':'')+'</button>').join('');
 $('ends').style.display=j.full360?'none':'';}
function show(j){
 if(first){['home','back','tlo','thi','lm','off','lat','lon','bright','server','port','user','topic','units'].forEach(k=>{$(k).value=(typeof j[k]=='number'&&!['lm','port','bright'].includes(k))?j[k].toFixed(k=='lat'||k=='lon'?4:1):j[k];});first=false;}
 $('st').textContent=(j.test?'Tuning':'Live')+' · '+(j.v==null?'no reading yet':j.v.toFixed(1)+' '+I.unit)+
  (j.homed?'':' · ⚠ homing failed')+(j.dirty?' · tuning not saved':'')+(j.extra?' · '+j.extra:'');
 $('lt').textContent='Backlight is '+(j.lit?'on':'off')+(j.lm==2?(j.dusk>=0?' · dusk today '+hm(j.dusk)+', off at '+j.off:' · waiting for the clock'):'');
 let f=!j.mqtt?'Not connected to the MQTT server':j.age<0?'Connected, no messages yet on this topic':
  (j.ok?'✓ Data arriving: last message '+j.age+' s ago':'Messages arriving ('+j.age+' s ago) but no reading found in them: check the topic and units');
 $('feedst').textContent=f;$('feedst').className=(j.mqtt&&j.ok)?'st':'bad';}
function go(u){return fetch(u).then(r=>r.json()).then(show).catch(()=>{$('st').textContent='No answer: homing, restarting, or the page has closed (restart the gauge or press BOOT).';});}
function setv(k){go('/set?'+k+'='+encodeURIComponent($(k).value));}
function lights(){go('/lights?'+q({mode:$('lm').value,off:$('off').value,lat:$('lat').value,lon:$('lon').value,bright:$('bright').value}));}
function feed(){let o={server:$('server').value,port:$('port').value,user:$('user').value,topic:$('topic').value,units:$('units').value};
 if($('pass').value)o.pass=$('pass').value;go('/feed?'+q(o));}
function gtypeSet(){if(confirm('Change gauge type and restart?'))fetch('/type?t='+$('gtype').value).then(()=>{$('st').textContent='Restarting… reload this page in a minute (the address changes with the type).';});}
fetch('/info').then(r=>r.json()).then(j=>{info(j);go('/status');});
setInterval(()=>go('/status'),3000);
</script></body></html>)HTML";

void setupWeb() {
  web.on("/", []() { webOffAt = millis() + webOpenMs; web.send(200, "text/html; charset=utf-8", SETTINGS_PAGE); });
  web.on("/info", sendInfo);
  web.on("/status", sendStatus);

  web.on("/go", []() { queueTest({ web.arg("v").toFloat() }); sendStatus(); });
  web.on("/sweep", []() {
    if (P().full360) queueTest({ 0, 180, 0 });
    else             queueTest({ minVal(), maxVal(), topValue() });
    sendStatus();
  });
  web.on("/set", []() {
    if (web.hasArg("home")) {              // new offset: find the magnet again and park at the top
      ts.homeOffsetDeg = constrain(web.arg("home").toFloat(), -180.0f, 180.0f);
      testMode = true; testSinceMs = millis(); testCount = 0;
      rehomeRequested = true;
    }
    if (web.hasArg("back")) {              // top from the left, pause, then from the right
      ts.backlashDeg = constrain(web.arg("back").toFloat(), 0.0f, 20.0f);
      queueTest({ degToValue(-30), topValue(), degToValue(40), topValue() });
    }
    if (web.hasArg("tlo")) {
      ts.trimLowDeg = constrain(web.arg("tlo").toFloat(), -15.0f, 15.0f);
      queueTest({ degToValue(P().deg[0] + 25), minVal() });
    }
    if (web.hasArg("thi")) {
      ts.trimHighDeg = constrain(web.arg("thi").toFloat(), -15.0f, 15.0f);
      queueTest({ degToValue(P().deg[P().nPoints - 1] - 25), maxVal() });
    }
    tuneDirty = true;
    sendStatus();
  });
  web.on("/save", []() { saveTypeSettings(cfg.gaugeType, ts); tuneDirty = false; sendStatus(); });
  web.on("/live", []() { endTest(); sendStatus(); });
  web.on("/rehome", []() { rehomeRequested = true; testMode = true; testSinceMs = millis(); testCount = 0; sendStatus(); });

  web.on("/lights", []() {                 // takes effect and is saved straight away
    if (web.hasArg("mode")) cfg.lightsMode = constrain(web.arg("mode").toInt(), 0, 2);
    int h, m;
    if (web.hasArg("off") && sscanf(web.arg("off").c_str(), "%d:%d", &h, &m) == 2) {
      cfg.offHour = constrain(h, 0, 23); cfg.offMinute = constrain(m, 0, 59);
    }
    if (web.hasArg("lat")) cfg.latitude  = constrain(web.arg("lat").toFloat(), -90.0f, 90.0f);
    if (web.hasArg("lon")) cfg.longitude = constrain(web.arg("lon").toFloat(), -180.0f, 180.0f);
    if (web.hasArg("bright")) cfg.brightness = constrain(web.arg("bright").toInt(), 0, 255);
    saveConfig();
    backlightOn = !backlightOn;            // force a redraw
    updateBacklightState();
    ledsDirty = true;
    sendStatus();
  });

  web.on("/feed", []() {                   // saved straight away, then reconnects
    if (web.hasArg("server")) strlcpy(cfg.mqttServer, web.arg("server").c_str(), sizeof(cfg.mqttServer));
    if (web.hasArg("port"))   { int p = web.arg("port").toInt(); cfg.mqttPort = p > 0 ? p : 1883; }
    if (web.hasArg("user"))   strlcpy(cfg.mqttUser, web.arg("user").c_str(), sizeof(cfg.mqttUser));
    if (web.hasArg("pass"))   strlcpy(cfg.mqttPass, web.arg("pass").c_str(), sizeof(cfg.mqttPass));
    if (web.hasArg("topic"))  strlcpy(ts.topic, web.arg("topic").c_str(), sizeof(ts.topic));
    if (web.hasArg("units") && unitAllowed(cfg.gaugeType, web.arg("units").c_str()))
      strlcpy(ts.units, web.arg("units").c_str(), sizeof(ts.units));
    saveConfig();
    saveTypeSettings(cfg.gaugeType, ts);
    lastMsgMs = 0; lastMsgHadValue = false; msgCount = 0;
    connectMqtt();
    sendStatus();
  });

  web.on("/led", []() {
    finderLed = constrain(web.arg("n").toInt(), 0, NUM_LEDS - 1);
    finderUntil = millis() + 5000;
    ledsDirty = true;
    sendStatus();
  });

  web.on("/type", []() {
    int t = web.arg("t").toInt();
    web.send(200, "application/json", "{}");
    if (t >= 0 && t < G_COUNT && t != cfg.gaugeType) {
      cfg.gaugeType = t;
      saveConfig();
      delay(300);
      ESP.restart();
    }
  });

  openWeb();
  MDNS.addService("http", "tcp", 80);
}

// Called from loop() when the pointer is idle: visit queued test points
void serviceTests() {
  if (!testMode) return;
  if (millis() - testSinceMs > testTimeoutMs) { endTest(); return; }
  if (testCount == 0) return;
  if (testDwellUntil == 0) { testDwellUntil = millis() + 1500; return; }   // pause to look
  if ((long)(millis() - testDwellUntil) < 0) return;
  float v = testQueue[0];
  for (int i = 1; i < testCount; i++) testQueue[i - 1] = testQueue[i];
  testCount--;
  testDwellUntil = 0;
  powerCoils();
  moveToSteps(valueToSteps(v));
}

// ===========================================================================
// SETUP
// ===========================================================================
void setup() {
  Serial.begin(115200);
  strip.begin();
  strip.setBrightness(255);                // backlight is dimmed per LED instead
  STATUS_STARTED;
  Serial.println("\nOpen Gauges");

  char idBuf[5];
  snprintf(idBuf, sizeof(idBuf), "%04x", (uint16_t)(ESP.getEfuseMac() >> 32));
  deviceId = idBuf;

  pinMode(HALL_PWR, OUTPUT);
  hallPower(false);
  loadConfig();

  // The C3 Super Mini's tiny antenna (and its power supply) cope better at low power
  WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t) { WiFi.setTxPower(WIFI_POWER_8_5dBm); }, ARDUINO_EVENT_WIFI_STA_START);
  WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t) { WiFi.setTxPower(WIFI_POWER_8_5dBm); }, ARDUINO_EVENT_WIFI_AP_START);

  bool forceSetup = setupButtonHeld();     // red ring = the moment to hold BOOT
  connectWiFi(forceSetup);
  Serial.printf("Gauge: %s\n", P().title);
  setupOTA();
  setupWeb();

  // Home, measure one turn, park at 12 o'clock
  STATUS_HOMING;
  homeGauge(true, true);
  lastRehomeMs = millis();

  // Self-test
  STATUS_SELFTEST;
  if (P().full360) {
    delay(2000);                           // at North: check it against the N mark
    runToPosition(valueToSteps(180)); delay(1000);
    runToPosition(valueToSteps(0));
  } else {
    const float pts[3] = { minVal(), maxVal(), topValue() };
    for (float v : pts) {
      if (P().extra == EX_DAYMAX) { markerLed = valueToLed(v); updateLeds(); }   // LED should sit beside the pointer
      runToPosition(valueToSteps(v));
      delay(P().extra == EX_DAYMAX ? 1500 : 1000);
    }
    markerLed = -1;
  }

  // Clock: dusk, trend history, day's maximum
  configTzTime(cfg.tz, "pool.ntp.org", "time.google.com");
  for (int t = 0; t < 20 && time(nullptr) < 1700000000; t++) delay(500);
  if (P().extra == EX_TREND)  { loadHistory(); slotStartMs = millis(); }
  if (P().extra == EX_DAYMAX) loadDayMax();

  client.setBufferSize(4096);              // weeWX loop packets are bigger than the default
  client.setCallback(mqttCallback);
  connectMqtt();

  backlightOn = !backlightOn;              // force the first update
  updateBacklightState();
  updateLeds();
  Serial.printf("Ready. Settings page: http://%s.local  or  http://%s\n",
                hostName().c_str(), WiFi.localIP().toString().c_str());
}

// ===========================================================================
// LOOP — while the pointer moves, do nothing but step (smooth motion)
// ===========================================================================
void loop() {
  if (stepper.distanceToGo() != 0) { stepper.run(); return; }
  if (continueMove()) return;

  if (coilsOn) { stepper.disableOutputs(); coilsOn = false; }   // motor stays cool; gearbox holds

  static unsigned long lastNet = 0;
  if (millis() - lastNet >= 50) {
    lastNet = millis();
    if (WiFi.status() == WL_CONNECTED) ArduinoOTA.handle();
    serviceMqtt();
    if (webOn) {
      web.handleClient();
      if ((long)(millis() - webOffAt) > 0 && !testMode) {
        web.stop(); webOn = false;
        Serial.println("Settings page closed (restart or press BOOT to open it again)");
      }
    }
  }

  if (P().extra == EX_TREND) updateHistorySlot();

  static unsigned long lastMinute = 0;
  if (millis() - lastMinute > 30000) {
    lastMinute = millis();
    updateBacklightState();
    if (P().extra == EX_DAYMAX) checkNewDay();
  }

  if (finderLed >= 0 && (long)(millis() - finderUntil) > 0) { finderLed = -1; ledsDirty = true; }

  static unsigned long lastLed = 0;
  if (ledsDirty || (P().extra == EX_TREND && trendFast && millis() - lastLed > 30)) {
    lastLed = millis(); ledsDirty = false;
    updateLeds();
  }

  if (rehomeRequested || millis() - lastRehomeMs > (homed ? rehomeIntervalMs : rehomeRetryMs)) {
    rehomeRequested = false;
    rehome();
    lastRehomeMs = millis();
  }

  serviceTests();

  // BOOT pressed while running: open the settings page again (ring flashes cyan)
  static unsigned long bootDownMs = 0;
  if (digitalRead(SETUP_BTN_PIN) == LOW) {
    if (!bootDownMs) bootDownMs = millis();
    else if (bootDownMs != 1 && millis() - bootDownMs > 100) {
      bootDownMs = 1;
      openWeb();
      STATUS_SETUP; delay(600); ledsDirty = true;
    }
  } else bootDownMs = 0;

  // Follow the feed
  const Profile& p = P();
  if (!testMode && !isnan(currentVal)) {
    float change = p.full360 ? fabsf(angleDiff(currentVal, shownVal)) : fabsf(currentVal - shownVal);
    if (isnan(shownVal) || change >= p.deadband) {
      powerCoils();
      moveToSteps(valueToSteps(currentVal));
      shownVal = currentVal;
    }
  }

  stepper.run();
}
