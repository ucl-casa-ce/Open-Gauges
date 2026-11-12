/*
  Open Gauges — Air Pressure gauge
  Board: ESP32-C3 Super Mini (Arduino core for ESP32 v3.x)
  Motor: 28BYJ-48 + ULN2003 driver   Homing: KY-003 hall sensor + 5 mm magnet
  LEDs:  WS2812 / NeoPixel strip around the rim

  - Reads barometric pressure from the weeWX MQTT loop packet (JSON)
  - Dial is NOT a full sweep: 950 hPa (bottom-left) -> 1000 (top) -> 1050 (bottom-right)
    = 270 degrees. The pointer never takes the short path through the bottom gap.
  - Works out the 3-hour pressure tendency ON BOARD from the readings it receives,
    and lights the LEDs next to "RISING" or "FALLING". A rapid change makes them pulse.
  - The trend history is saved to flash, so a reboot or power cut doesn't lose it.
  - Start-up progress on the ring: RED started, BLUE Wi-Fi, YELLOW homing,
    PURPLE self-test, WHITE ready.
  - The hall sensor is powered from GPIO5 only while homing (wire KY-003 "+" to GPIO5,
    not to 3V3/5V), so it draws nothing the rest of the time.

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

// The weeWX loop packet (JSON). The sketch looks for barometer_mbar / barometer_inHg /
// barometer / pressure... keys and converts inHg to hPa automatically.
// A topic that carries a plain number (e.g. ".../barometer_mbar") also works.
const char* pressure_topic = "personal/ucfnaps/downhamweather/loop";   // edit for your station

// ===========================================================================
// PINS — ESP32-C3 Super Mini (avoid strapping pins GPIO2, GPIO8, GPIO9)
// ===========================================================================
#define IN1_PIN   4     // ULN2003 IN1
#define IN2_PIN   3     // ULN2003 IN2
#define IN3_PIN   1     // ULN2003 IN3
#define IN4_PIN   0     // ULN2003 IN4
#define HALL_PIN  7     // KY-003 S
#define HALL_PWR  5     // KY-003 + (VCC) — powered from this pin only while homing
#define LED_PIN   10    // LED strip DIN

// ===========================================================================
// DIAL GEOMETRY
// ===========================================================================
const float dialMinHpa    = 950.0;    // bottom-left end of the scale
const float dialMaxHpa    = 1050.0;   // bottom-right end of the scale
const float dialTopHpa    = 1000.0;   // value at 12 o'clock
const float dialSweepDeg  = 270.0;    // 950 -> 1050
const float degPerHpa     = dialSweepDeg / (dialMaxHpa - dialMinHpa);   // 2.7 deg per hPa

// How far the pointer must travel FORWARD (clockwise) from the magnet trigger
// point to sit exactly on the 12 o'clock (1000 hPa) mark. Same idea as the wind gauge:
//   stops short (anticlockwise of 1000) -> increase    overshoots -> decrease
const float homeOffsetDeg = 13.0;

// BACKLASH: the 28BYJ-48 gearbox has a few degrees of play. Homing approaches
// clockwise, so clockwise moves land accurately but anticlockwise moves stop short.
// Fix: every anticlockwise move overshoots by this much, then comes back clockwise,
// so the pointer always arrives from the same side.
//   Still short after anticlockwise moves -> increase   (typical 2-6 deg)
const float backlashDeg = 4.0;

// Don't nudge the pointer for changes smaller than this (0.1 hPa = 0.27 deg)
const float moveDeadbandHpa = 0.1;

// ===========================================================================
// TREND (3-hour pressure tendency, Met Office style)
// ===========================================================================
// 3-hour change needed to light RISING / FALLING (hPa). Met Office: <0.1 steady,
// 0.1-1.5 slowly, 1.6-3.5 rising/falling, 3.6-6.0 quickly, >6.0 very rapidly.
const float trendOnHpa      = 0.5;    // light the LEDs at this change or more
const float trendOffHpa     = 0.3;    // turn them off below this (hysteresis, stops flicker)
const float trendFastHpa    = 3.6;    // "quickly" or faster -> LEDs pulse
const int   minHistoryMins  = 60;     // need this much history before showing a trend

// ===========================================================================
// LEDS
// ===========================================================================
#define NUM_LEDS        84
#define LED_BRIGHTNESS    50         // 0-255, backlight ring only
#define TREND_BRIGHTNESS  255        // 0-255, RISING / FALLING LEDs when lit (and fade peak)
#define TREND_FADE_MIN    30         // 0-255, lowest point of the fade for a rapid change

// LED numbers to light beside the words. Defaults assume LED 0 is at 12 o'clock and
// the strip runs clockwise (80 LEDs = 4.5 deg each): RISING ~162 deg, FALLING ~198 deg.
// Set LED_FINDER_MODE to 1 once to see which numbers are where on YOUR strip.
const int risingLeds[]  = {83, 0, 1, 2};
const int fallingLeds[] = {70, 71, 72, 73, 74};

const uint8_t backlightRGB[3] = {255, 255, 255};   // rest of the ring
const uint8_t risingRGB[3]    = {0,   200, 60};    // green
const uint8_t fallingRGB[3]   = {255, 70,  0};     // amber
const float   pulsePeriodS    = 2.0;               // fade in/out period for a rapid change

// 1 = boot into LED finder: lights each LED in turn (red) and prints its number.
#define LED_FINDER_MODE 0

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
Preferences  prefs;

// ===========================================================================
// STATE
// ===========================================================================
float currentHpa = NAN;     // smoothed latest reading
float shownHpa   = NAN;     // value the pointer was last sent to
bool  coilsOn    = true;
unsigned long lastRehomeMs = 0;

// History: one average per 10-minute slot, 18 slots = 3 hours
const unsigned long slotMs = 10UL * 60UL * 1000UL;
const int SLOTS = 18;
float history[SLOTS];
int   histCount = 0, histHead = 0;
float slotSum = 0;  uint16_t slotN = 0;
unsigned long slotStartMs = 0;

enum Trend { TREND_STEADY, TREND_RISING, TREND_FALLING };
Trend trend = TREND_STEADY;
bool  trendFast = false;
float trendChange3h = 0;
bool  ledsDirty = true;

// ===========================================================================
// HELPERS
// ===========================================================================
bool atMagnet() { return digitalRead(HALL_PIN) == LOW; }   // KY-003 pulls LOW on magnet

// The hall sensor (and its on-board LED) is only powered while homing.
// When off, the signal pin is left floating-free (plain INPUT) so the internal
// pull-up can't leak current back through the module's own pull-up resistor.
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

long hpaToSteps(float hpa) {
  hpa = constrain(hpa, dialMinHpa, dialMaxHpa);
  return lround((hpa - dialTopHpa) * degPerHpa * stepsPerDegree);   // 0 = 12 o'clock
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
  runToPosition(0);          // park at 1000 hPa
  shownHpa = NAN;            // force a fresh move to the current reading
}

// ===========================================================================
// LEDS
// ===========================================================================
void setRGB(int i, const uint8_t rgb[3], float k = 1.0) {
  if (i < 0 || i >= NUM_LEDS) return;
  strip.setPixelColor(i, strip.Color(rgb[0] * k, rgb[1] * k, rgb[2] * k));
}

void updateLeds() {
  // Strip brightness is left at 255; the backlight is dimmed here instead,
  // so the RISING / FALLING LEDs can run at full brightness.
  const float backK = LED_BRIGHTNESS / 255.0f;
  for (int i = 0; i < NUM_LEDS; i++) setRGB(i, backlightRGB, backK);

  // Rising / falling: steady on at TREND_BRIGHTNESS.
  // Rapid change: fade between TREND_FADE_MIN and TREND_BRIGHTNESS.
  float level = TREND_BRIGHTNESS;
  if (trendFast) {
    float wave = 0.5f + 0.5f * sinf(millis() / 1000.0f * TWO_PI / pulsePeriodS);   // 0..1
    level = TREND_FADE_MIN + (TREND_BRIGHTNESS - TREND_FADE_MIN) * wave;
  }
  const float k = level / 255.0f;

  if (trend == TREND_RISING)
    for (int i : risingLeds)  setRGB(i, risingRGB, k);
  if (trend == TREND_FALLING)
    for (int i : fallingLeds) setRGB(i, fallingRGB, k);

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

void ledFinder() {
  Serial.println("LED FINDER MODE — note the numbers beside RISING and FALLING");
  for (;;) {
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.clear();
      strip.setPixelColor(i, strip.Color(255, 0, 0));
      strip.show();
      Serial.printf("LED %d\n", i);
      delay(1000);
    }
  }
}

// ===========================================================================
// TREND
// ===========================================================================
void saveHistory() {
  time_t now = time(nullptr);
  if (now < 1700000000) return;              // no clock yet
  prefs.putBytes("hist", history, sizeof(history));
  prefs.putInt("count", histCount);
  prefs.putInt("head", histHead);
  prefs.putULong("ts", (uint32_t)now);
}

void loadHistory() {
  // Only reuse saved history if it's recent (rebooted within 20 minutes)
  time_t now = time(nullptr);
  uint32_t ts = prefs.getULong("ts", 0);
  if (now < 1700000000 || ts == 0 || now - ts > 20 * 60) {
    Serial.println("No recent trend history saved; building it from scratch.");
    return;
  }
  prefs.getBytes("hist", history, sizeof(history));
  histCount = prefs.getInt("count", 0);
  histHead  = prefs.getInt("head", 0);
  Serial.printf("Restored %d trend samples (%d min).\n", histCount, histCount * 10);
}

void pushHistory(float v) {
  history[histHead] = v;
  histHead = (histHead + 1) % SLOTS;
  if (histCount < SLOTS) histCount++;
  saveHistory();
}

void computeTrend() {
  Trend old = trend;
  bool oldFast = trendFast;

  int spanMins = histCount * 10;
  if (isnan(currentHpa) || spanMins < minHistoryMins) {
    trend = TREND_STEADY; trendFast = false;
  } else {
    float oldest = history[(histHead - histCount + SLOTS) % SLOTS];
    trendChange3h = (currentHpa - oldest) * (180.0f / spanMins);   // scale to 3 h
    float mag = fabsf(trendChange3h);

    if (trend == TREND_STEADY) {
      if (mag >= trendOnHpa) trend = trendChange3h > 0 ? TREND_RISING : TREND_FALLING;
    } else {
      if (mag < trendOffHpa) trend = TREND_STEADY;
      else trend = trendChange3h > 0 ? TREND_RISING : TREND_FALLING;
    }
    trendFast = (trend != TREND_STEADY) && mag >= trendFastHpa;
  }

  if (trend != old || trendFast != oldFast) {
    ledsDirty = true;
    Serial.printf("Trend: %s%s (%.2f hPa / 3 h)\n",
                  trend == TREND_RISING ? "RISING" : trend == TREND_FALLING ? "FALLING" : "STEADY",
                  trendFast ? " fast" : "", trendChange3h);
  }
}

void updateHistorySlot() {
  if (millis() - slotStartMs < slotMs) return;
  slotStartMs += slotMs;
  if (slotN > 0) {
    pushHistory(slotSum / slotN);
    Serial.printf("Trend sample stored: %.2f hPa (%d min of history)\n", slotSum / slotN, histCount * 10);
  }
  slotSum = 0; slotN = 0;
  computeTrend();
}

// ===========================================================================
// MQTT
// ===========================================================================
float readNumber(JsonVariant v) {
  if (v.is<const char*>()) return atof(v.as<const char*>());
  if (v.is<float>() || v.is<long>()) return v.as<float>();
  return NAN;
}

float extractPressureHpa(byte* payload, unsigned int length) {
  // Plain number payload
  unsigned int i = 0;
  while (i < length && isspace(payload[i])) i++;
  if (i < length && payload[i] != '{') {
    char buf[24]; unsigned int n = min(length - i, (unsigned int)sizeof(buf) - 1);
    memcpy(buf, payload + i, n); buf[n] = 0;
    float v = atof(buf);
    return v < 40 ? v * 33.8639f : v;
  }

  // JSON loop packet
  JsonDocument doc;
  if (deserializeJson(doc, (const char*)payload, length)) return NAN;
  const char* keys[] = { "barometer_mbar", "barometer_hPa", "barometer_inHg", "barometer",
                         "pressure_mbar",  "pressure_hPa",  "pressure_inHg",  "pressure" };
  for (const char* key : keys) {
    JsonVariant v = doc[key];
    if (v.isNull()) continue;
    float val = readNumber(v);
    if (isnan(val)) continue;
    if (strstr(key, "inHg") || val < 40) val *= 33.8639f;   // inHg -> hPa
    return val;
  }
  return NAN;
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  float hpa = extractPressureHpa(payload, length);
  if (isnan(hpa) || hpa < 870 || hpa > 1090) return;   // ignore junk

  currentHpa = isnan(currentHpa) ? hpa : currentHpa + 0.2f * (hpa - currentHpa);  // light smoothing
  slotSum += hpa; slotN++;
  computeTrend();

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 60000) {
    lastPrint = millis();
    Serial.printf("Pressure %.1f hPa\n", currentHpa);
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
    String id = "OpenGauge-Pressure-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    Serial.print("Connecting to MQTT... ");
    if (client.connect(id.c_str())) {
      Serial.println("connected");
      client.subscribe(pressure_topic);
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
  Serial.println("\nOpen Gauges — Air Pressure");

  pinMode(HALL_PWR, OUTPUT);
  hallPower(false);                    // sensor off until it's needed
  if (LED_FINDER_MODE) ledFinder();

  // Wi-Fi first, before the motor runs, so the two big power draws never overlap
  STATUS_WIFI;
  connectWiFi();

  // Home, calibrate, park at 1000 hPa (12 o'clock)
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
  Serial.println("Self-test: 950 -> 1050 -> 1000 hPa");
  STATUS_SELFTEST;
  runToPosition(hpaToSteps(dialMinHpa));  delay(1000);
  runToPosition(hpaToSteps(dialMaxHpa));  delay(1000);
  runToPosition(hpaToSteps(dialTopHpa));

  // Clock (only used to decide whether saved trend history is still fresh)
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  for (int t = 0; t < 20 && time(nullptr) < 1700000000; t++) delay(500);
  prefs.begin("pgauge", false);
  loadHistory();
  slotStartMs = millis();

  client.setServer(mqtt_server, mqtt_port);
  client.setBufferSize(4096);          // weeWX loop packets are larger than the 256-byte default
  client.setCallback(mqttCallback);

  updateLeds();                        // normal white backlight = ready
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

  updateHistorySlot();

  if (millis() - lastRehomeMs > rehomeIntervalMs) {
    rehome();
    lastRehomeMs = millis();
  }

  static unsigned long lastLed = 0;
  if (ledsDirty || (trendFast && millis() - lastLed > 30)) {
    lastLed = millis(); ledsDirty = false;
    updateLeds();
  }

  // Move the pointer if the reading has changed enough
  if (!isnan(currentHpa) && (isnan(shownHpa) || fabsf(currentHpa - shownHpa) >= moveDeadbandHpa)) {
    powerCoils();
    moveToSteps(hpaToSteps(currentHpa));
    shownHpa = currentHpa;
  }

  stepper.run();
}
