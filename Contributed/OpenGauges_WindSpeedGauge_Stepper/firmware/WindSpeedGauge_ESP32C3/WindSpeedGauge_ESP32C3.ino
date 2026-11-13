/*
  Open Gauges — Wind Speed gauge
  Board: ESP32-C3 Super Mini (Arduino core for ESP32 v3.x)
  Motor: 28BYJ-48 + ULN2003 driver   Homing: KY-003 hall sensor + 5 mm magnet
  LEDs:  WS2812 / NeoPixel strip around the rim (white backlight)

  - Reads the 1-minute wind gust (mph) from MQTT — a plain number, or JSON. No averaging,
    so the pointer shows every gust as it arrives.
  - Dial is NOT a full sweep: 0 mph (bottom-left) -> 20 (top) -> 40 (bottom-right)
    = 270 degrees. The pointer never takes the short path through the bottom gap.
  - Tracks the day's maximum and lights the LED beside it on the dial (red). Resets at
    local midnight; saved to flash, so a reboot or power cut doesn't lose it.
  - Start-up progress on the ring: RED started, BLUE Wi-Fi, YELLOW homing,
    PURPLE self-test, WHITE ready.
  - Same wiring as the Open Gauges wind direction, pressure and temperature gauges.
  - KY-003 "+" can go to 5V (simplest), or to GPIO5 so the sensor is powered only
    while homing and draws nothing the rest of the time. The sketch works either way.

  Libraries (Library Manager): AccelStepper, PubSubClient, ArduinoJson (v7), Adafruit NeoPixel
  Tools menu: Board "ESP32C3 Dev Module", USB CDC On Boot "Enabled" (for Serial Monitor)
*/

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <AccelStepper.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>
#include <time.h>

// ===========================================================================
// WI-FI / MQTT
// ===========================================================================
const char* ssid        = "YOUR_WIFI_NAME";
const char* password    = "YOUR_WIFI_PASSWORD";

const char* mqtt_server = "mqtt.cetools.org";
const int   mqtt_port   = 1883;

// A topic carrying a plain number in mph (or km/h, m/s, knots if the topic name ends in
// "_kph", "_mps" or "_knot"). JSON also works: see the keys in extractSpeedMph().
const char* speed_topic = "personal/ucfnaps/downhamweather/windgust1min";   // edit for your station

// Units of a JSON key with no unit suffix. weeWX stores US units by default,
// so it is usually mph. Options: "mph", "kph", "mps", "knot"
const char* bareKeyUnits = "mph";

// Local time zone, for resetting the day's maximum at midnight (UK: GMT/BST)
const char* tzInfo = "GMT0BST,M3.5.0/1,M10.5.0";

// ===========================================================================
// PINS — ESP32-C3 Super Mini (avoid strapping pins GPIO2, GPIO8, GPIO9)
// ===========================================================================
#define IN1_PIN   4     // ULN2003 IN1
#define IN2_PIN   3     // ULN2003 IN2
#define IN3_PIN   1     // ULN2003 IN3
#define IN4_PIN   0     // ULN2003 IN4
#define HALL_PIN  7     // KY-003 S
#define HALL_PWR  5     // optional: KY-003 + here to power it only while homing
#define LED_PIN   10    // LED strip DIN

// ===========================================================================
// DIAL GEOMETRY
// ===========================================================================
const float dialMinMph   = 0.0;      // bottom-left end of the scale
const float dialMaxMph   = 40.0;     // bottom-right end of the scale
const float dialTopMph   = 20.0;     // value at 12 o'clock
const float dialSweepDeg = 270.0;    // 0 -> 40
const float degPerMph    = dialSweepDeg / (dialMaxMph - dialMinMph);   // 6.75 deg per mph

// How far the pointer must travel FORWARD (clockwise) from the magnet trigger
// point to sit exactly on the 12 o'clock (20 mph) mark. Same idea as the wind gauge:
//   stops short (anticlockwise of 20) -> increase    overshoots -> decrease
const float homeOffsetDeg = 13.0;

// BACKLASH: the 28BYJ-48 gearbox has a few degrees of play. Homing approaches
// clockwise, so clockwise moves land accurately but anticlockwise moves stop short.
// Fix: every anticlockwise move overshoots by this much, then comes back clockwise,
// so the pointer always arrives from the same side.
//   Still short after anticlockwise moves -> increase   (typical 2-6 deg)
const float backlashDeg = 4.0;

// Don't nudge the pointer for changes smaller than this (0.2 mph = 1.4 deg)
const float moveDeadbandMph = 0.2;

// ===========================================================================
// LEDS
// ===========================================================================
#define NUM_LEDS        84
#define LED_BRIGHTNESS  50           // 0-255; turn it down if the board resets
#define MAX_BRIGHTNESS  255          // 0-255, the day's-maximum LED
const uint8_t backlightRGB[3] = {255, 255, 255};
const uint8_t maxRGB[3]       = {255, 0, 0};      // red

// Where the LEDs sit, so the right one lights beside the day's maximum.
// LED 0 is at the gap at the bottom of the dial, just right of 6 o'clock, and the strip
// runs anticlockwise from there (the same strip as the pressure gauge).
//   Bearings are degrees clockwise from 12 o'clock (so 6 o'clock = 180).
//   At start-up the marker LED follows the pointer through the self-test: if it sits
//   beside the pointer the numbers are right. LED ahead of the pointer clockwise ->
//   decrease led0BearingDeg; behind it -> increase.
const float led0BearingDeg = 160.0;   // where LED 0 sits
const float ledsPerMetre   = 144.0;   // strip density
const float stripRadiusMm  = 95.0;    // radius of the strip in the case's LED channel
const int   ledDirection   = -1;      // -1 = strip runs anticlockwise from LED 0, +1 = clockwise
// 144/m round a 95 mm radius = ~86 LEDs per full turn, ~4.2 deg apart
const float degPerLed      = 360.0 / (2.0 * PI * stripRadiusMm / 1000.0 * ledsPerMetre);

// ===========================================================================
// MOTOR
// ===========================================================================
long  stepsPerRevolution = 4096;                       // overwritten by calibration
float stepsPerDegree     = stepsPerRevolution / 360.0;
const float homingSpeed  = 300.0;
const float runSpeed     = 400.0;
const float runAccel     = 150.0;
const bool  releaseCoilsWhenIdle = true;   // motor stays cool; gearbox holds the pointer
const unsigned long rehomeIntervalMs = 6UL * 60UL * 60UL * 1000UL;  // re-zero every 6 h

AccelStepper stepper(AccelStepper::HALF4WIRE, IN1_PIN, IN3_PIN, IN2_PIN, IN4_PIN);
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
WiFiClient   espClient;
PubSubClient client(espClient);

// ===========================================================================
// STATE
// ===========================================================================
float currentMph = NAN;     // latest reading
float shownMph   = NAN;     // value the pointer was last sent to
bool  coilsOn    = true;
unsigned long lastRehomeMs = 0;

Preferences prefs;
float dayMaxMph = NAN;      // today's maximum
long  maxDay    = -1;       // the day it belongs to, as yyyymmdd
int   markerLed = -1;       // LED lit during the self-test (-1 = none)
bool  ledsDirty = true;

// ===========================================================================
// HELPERS
// ===========================================================================
bool atMagnet() { return digitalRead(HALL_PIN) == LOW; }   // KY-003 pulls LOW on magnet

// The hall sensor (and its on-board LED) is only powered while homing when wired to GPIO5.
// When off, the signal pin is left as plain INPUT so the internal pull-up can't leak
// current back through the module's own pull-up resistor.
void hallPower(bool on) {
  if (on) {
    digitalWrite(HALL_PWR, HIGH);
    pinMode(HALL_PIN, INPUT_PULLUP);
    delay(20);                                // let the sensor settle
  } else {
    pinMode(HALL_PIN, INPUT);
    digitalWrite(HALL_PWR, LOW);
  }
}

long mphToSteps(float mph) {
  mph = constrain(mph, dialMinMph, dialMaxMph);
  return lround((mph - dialTopMph) * degPerMph * stepsPerDegree);   // 0 = 12 o'clock
}

void applyHomeOffset() {
  stepper.setCurrentPosition(-lround(homeOffsetDeg * stepsPerDegree));
}

// Start a move that always finishes travelling clockwise (takes up gear backlash)
long finalTarget = 0;
bool finalPending = false;

void moveToSteps(long target) {
  if (target < stepper.currentPosition()) {          // anticlockwise move
    stepper.moveTo(target - lround(backlashDeg * stepsPerDegree));   // overshoot
    finalTarget = target;
    finalPending = true;                              // then come back clockwise
  } else {
    stepper.moveTo(target);
    finalPending = false;
  }
}

// Called when a move ends: if it was the overshoot, start the clockwise return
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
// HOMING & CALIBRATION
// ===========================================================================
// Leaves the hall sensor powered — the caller switches it off when finished
void seekMagnetAndZero() {
  hallPower(true);
  powerCoils();
  stepper.setMaxSpeed(homingSpeed);
  stepper.setSpeed(homingSpeed);
  while (atMagnet())  stepper.runSpeed();   // step off if already on it
  while (!atMagnet()) stepper.runSpeed();   // roll on to the leading edge
  applyHomeOffset();
}

void calibrateStepsPerRevolution() {
  Serial.println("Calibrating: one magnet -> magnet revolution...");
  stepper.setCurrentPosition(0);
  stepper.setSpeed(homingSpeed);
  while (atMagnet())  stepper.runSpeed();
  while (!atMagnet()) stepper.runSpeed();
  long measured = labs(stepper.currentPosition());
  if (measured > 100) {
    stepsPerRevolution = measured;
    stepsPerDegree = stepsPerRevolution / 360.0;
  } else {
    Serial.println("Calibration looked wrong; keeping default.");
  }
  applyHomeOffset();
  Serial.printf("Steps/rev: %ld  steps/deg: %.4f\n", stepsPerRevolution, stepsPerDegree);
}

void rehome() {
  Serial.println("Auto re-home...");
  seekMagnetAndZero();
  hallPower(false);
  stepper.setMaxSpeed(runSpeed);
  stepper.setAcceleration(runAccel);
  runToPosition(0);          // park at 20 mph
  shownMph = NAN;            // force a fresh move to the current reading
}

// ===========================================================================
// LEDS
// ===========================================================================
// LED beside a given speed on the dial
int mphToLed(float mph) {
  mph = constrain(mph, dialMinMph, dialMaxMph);
  float bearing = (mph - dialTopMph) * degPerMph;            // -135 .. 135
  float d = (bearing - led0BearingDeg) * ledDirection;       // degrees along the strip from LED 0
  d = fmodf(d, 360.0f); if (d < 0) d += 360.0f;
  int i = lround(d / degPerLed);
  int perTurn = lround(360.0f / degPerLed);
  return i >= perTurn ? i - perTurn : i;
}

void setRGB(int i, const uint8_t rgb[3], float k) {
  if (i < 0 || i >= NUM_LEDS) return;
  strip.setPixelColor(i, strip.Color(rgb[0] * k, rgb[1] * k, rgb[2] * k));
}

void updateLeds() {
  // Strip brightness is left at 255; the backlight is dimmed here instead,
  // so the maximum LED can run at full brightness.
  for (int i = 0; i < NUM_LEDS; i++) setRGB(i, backlightRGB, LED_BRIGHTNESS / 255.0f);
  int m = markerLed >= 0 ? markerLed : (isnan(dayMaxMph) ? -1 : mphToLed(dayMaxMph));
  if (m >= 0) setRGB(m, maxRGB, MAX_BRIGHTNESS / 255.0f);
  strip.show();
}

// Start-up progress shown on the whole ring (no Serial Monitor needed):
//   RED    = sketch has started
//   BLUE   = connecting to Wi-Fi
//   YELLOW = homing / calibrating (motor should be turning)
//   PURPLE = self-test sweep
//   WHITE  = ready (normal backlight)
void showStatus(uint8_t r, uint8_t g, uint8_t b) {
  strip.fill(strip.Color(r, g, b));
  strip.show();
}
#define STATUS_STARTED   showStatus(80, 0, 0)
#define STATUS_WIFI      showStatus(0, 0, 80)
#define STATUS_HOMING    showStatus(80, 60, 0)
#define STATUS_SELFTEST  showStatus(60, 0, 80)

// ===========================================================================
// DAY'S MAXIMUM
// ===========================================================================
// Today's date as yyyymmdd, or -1 if the clock isn't set yet
long todayKey() {
  struct tm t;
  if (!getLocalTime(&t, 10)) return -1;
  return (t.tm_year + 1900) * 10000L + (t.tm_mon + 1) * 100L + t.tm_mday;
}

// Start a fresh maximum when the date changes
void checkNewDay() {
  long d = todayKey();
  if (d > 0 && d != maxDay) {
    if (maxDay > 0) Serial.printf("New day: yesterday's max was %.1f mph\n", dayMaxMph);
    maxDay = d;
    dayMaxMph = NAN;
    prefs.putLong("day", maxDay);
    prefs.putFloat("max", NAN);
    ledsDirty = true;
  }
}

void updateDayMax(float mph) {
  checkNewDay();
  if (maxDay < 0) return;                    // no clock yet
  if (isnan(dayMaxMph) || mph > dayMaxMph) {
    bool ledMoves = isnan(dayMaxMph) || mphToLed(mph) != mphToLed(dayMaxMph);
    dayMaxMph = mph;
    prefs.putFloat("max", dayMaxMph);        // flash write only when the max goes up
    if (ledMoves) ledsDirty = true;
    Serial.printf("New max for today: %.1f mph\n", dayMaxMph);
  }
}

// After a reboot, carry on with today's maximum if one was saved
void loadDayMax() {
  long d = todayKey();
  if (d > 0 && prefs.getLong("day", -1) == d) {
    maxDay = d;
    dayMaxMph = prefs.getFloat("max", NAN);
    Serial.printf("Restored today's max: %.1f mph\n", dayMaxMph);
  }
}

// ===========================================================================
// MQTT
// ===========================================================================
float readNumber(JsonVariant v) {
  if (v.is<const char*>()) return atof(v.as<const char*>());
  if (v.is<float>() || v.is<long>()) return v.as<float>();
  return NAN;
}

// Convert a speed in the given units ("mph", "kph", "mps", "knot") to mph
float toMph(float v, const char* units) {
  if (strstr(units, "kph"))  return v * 0.621371f;
  if (strstr(units, "mps"))  return v * 2.236936f;
  if (strstr(units, "knot")) return v * 1.150779f;
  return v;                                            // mph
}

bool endsWith(const char* s, const char* suffix) {
  size_t ls = strlen(s), lf = strlen(suffix);
  return ls >= lf && strcmp(s + ls - lf, suffix) == 0;
}

float extractSpeedMph(const char* topic, byte* payload, unsigned int length) {
  // Plain number payload — units taken from the topic name
  unsigned int i = 0;
  while (i < length && isspace(payload[i])) i++;
  if (i >= length) return NAN;
  if (payload[i] != '{') {
    char buf[24]; unsigned int n = min(length - i, (unsigned int)sizeof(buf) - 1);
    memcpy(buf, payload + i, n); buf[n] = 0;
    if (!isdigit(buf[0]) && buf[0] != '.') return NAN;   // "None", "null"
    float v = atof(buf);
    if (endsWith(topic, "_kph"))  return toMph(v, "kph");
    if (endsWith(topic, "_mps"))  return toMph(v, "mps");
    if (endsWith(topic, "_knot")) return toMph(v, "knot");
    return v;
  }

  // JSON — keys in order of preference
  JsonDocument doc;
  if (deserializeJson(doc, (const char*)payload, length)) return NAN;
  const char* keys[] = { "windgust1min", "windGust1min", "windGust_mph", "windGust_kph",
                         "windGust_mps", "windGust_knot", "windGust", "value" };
  for (const char* key : keys) {
    JsonVariant v = doc[key];
    if (v.isNull()) continue;
    float val = readNumber(v);
    if (isnan(val)) continue;
    const char* u = strstr(key, "_") ? strstr(key, "_") + 1 : bareKeyUnits;
    return toMph(val, u);
  }
  return NAN;
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  float mph = extractSpeedMph(topic, payload, length);
  if (isnan(mph) || mph < 0 || mph > 200) return;   // ignore junk

  currentMph = mph;                           // no averaging: show every gust
  updateDayMax(mph);

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 60000) {
    lastPrint = millis();
    Serial.printf("Gust %.1f mph (today's max %.1f)\n", currentMph, dayMaxMph);
  }
}

void connectWiFi() {
  Serial.printf("Connecting to Wi-Fi %s", ssid);
  WiFi.mode(WIFI_STA);
  // ESP32-C3 Super Mini: the tiny on-board antenna often won't connect at full
  // transmit power (and the current spike can reset the board). 8.5 dBm fixes it.
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  // Keep trying rather than rebooting (a reboot would re-home and re-sweep the pointer)
  for (int t = 1; WiFi.status() != WL_CONNECTED; t++) {
    delay(500); Serial.print(".");
    if (t % 40 == 0) {                       // every 20 s, start the attempt again
      Serial.println("\nStill trying Wi-Fi...");
      WiFi.disconnect();
      WiFi.setTxPower(WIFI_POWER_8_5dBm);
      WiFi.begin(ssid, password);
    }
  }
  Serial.printf("\nWi-Fi connected, IP %s\n", WiFi.localIP().toString().c_str());
}

void serviceNetwork() {
  if (WiFi.status() != WL_CONNECTED) return;     // ESP32 reconnects by itself
  if (!client.connected()) {
    static unsigned long lastTry = 0;
    if (millis() - lastTry < 5000) return;       // retry every 5 s, never block
    lastTry = millis();
    String id = "OpenGauge-WindSpeed-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    Serial.print("Connecting to MQTT... ");
    if (client.connect(id.c_str())) {
      Serial.println("connected");
      client.subscribe(speed_topic);
    } else {
      Serial.printf("failed rc=%d\n", client.state());
    }
    return;
  }
  client.loop();
}

// ===========================================================================
// SETUP
// ===========================================================================
void setup() {
  Serial.begin(115200);
  strip.begin();
  strip.setBrightness(255);            // full; backlight is dimmed per-pixel in updateLeds()
  STATUS_STARTED;
  delay(1500);
  Serial.println("\nOpen Gauges — Wind Speed");

  pinMode(HALL_PWR, OUTPUT);
  hallPower(false);                    // sensor off until it's needed

  // Wi-Fi first, before the motor runs, so the two big power draws never overlap
  STATUS_WIFI;
  connectWiFi();

  // Home, calibrate, park at 20 mph (12 o'clock)
  STATUS_HOMING;
  seekMagnetAndZero();
  delay(1000);
  calibrateStepsPerRevolution();
  hallPower(false);                    // homing done — sensor off
  stepper.setMaxSpeed(runSpeed);
  stepper.setAcceleration(runAccel);
  runToPosition(0);
  lastRehomeMs = millis();

  // Self-test: full scale both ways (never through the bottom gap)
  Serial.println("Self-test: 0 -> 40 -> 20 mph");
  STATUS_SELFTEST;
  // The red marker LED follows the pointer: it should sit beside it at each stop
  const float testMph[3] = { dialMinMph, dialMaxMph, dialTopMph };
  for (float mph : testMph) {
    markerLed = mphToLed(mph); updateLeds();
    runToPosition(mphToSteps(mph));
    delay(1500);
  }
  markerLed = -1;

  // Clock (for the day's maximum)
  configTzTime(tzInfo, "pool.ntp.org", "time.google.com");
  for (int t = 0; t < 20 && todayKey() < 0; t++) delay(500);
  prefs.begin("wsgauge", false);
  loadDayMax();

  client.setServer(mqtt_server, mqtt_port);
  client.setBufferSize(4096);          // weeWX loop packets are larger than the 256-byte default
  client.setCallback(mqttCallback);

  updateLeds();                        // white backlight (+ today's max) = ready
  Serial.println("Ready.");
}

// ===========================================================================
// LOOP — while the pointer moves, do nothing but step (smooth motion)
// ===========================================================================
void loop() {
  if (stepper.distanceToGo() != 0) {
    stepper.run();
    return;
  }
  if (continueMove()) return;     // finish an anticlockwise move from the clockwise side

  if (coilsOn && releaseCoilsWhenIdle) { stepper.disableOutputs(); coilsOn = false; }

  static unsigned long lastNet = 0;
  if (millis() - lastNet >= 50) { lastNet = millis(); serviceNetwork(); }

  if (millis() - lastRehomeMs > rehomeIntervalMs) {
    rehome();
    lastRehomeMs = millis();
  }

  static unsigned long lastDayCheck = 0;
  if (millis() - lastDayCheck > 30000) { lastDayCheck = millis(); checkNewDay(); }

  if (ledsDirty) { ledsDirty = false; updateLeds(); }

  // Move the pointer if the reading has changed enough
  if (!isnan(currentMph) && (isnan(shownMph) || fabsf(currentMph - shownMph) >= moveDeadbandMph)) {
    powerCoils();
    moveToSteps(mphToSteps(currentMph));
    shownMph = currentMph;
  }

  stepper.run();
}
