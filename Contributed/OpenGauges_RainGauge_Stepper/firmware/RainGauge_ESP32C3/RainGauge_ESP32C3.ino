/*
  Open Gauges — Daily Rainfall gauge
  Board: ESP32-C3 Super Mini (Arduino core for ESP32 v3.x)
  Motor: 28BYJ-48 + ULN2003 driver   Homing: KY-003 hall sensor + 5 mm magnet
  LEDs:  WS2812 / NeoPixel strip around the rim (white backlight)

  - Reads today's rainfall (dayRain) from the weeWX MQTT loop packet (JSON)
  - Dial is NOT a full sweep: 0 mm (bottom-left) -> 50 mm (bottom-right) = 270 degrees.
    The scale is stretched at the low end so a few millimetres are easy to read:
      0-2 mm   at 5 units per mm
      2-10 mm  at 2 units per mm
      10-50 mm at 1 unit per mm      (66 units over 270 degrees; 17 mm at 12 o'clock)
    The pointer never takes the short path through the bottom gap.
  - The station resets dayRain at midnight, so the pointer returns to 0 by itself.
  - Start-up progress on the ring: RED started, BLUE Wi-Fi, YELLOW homing,
    PURPLE self-test, WHITE ready.
  - Same wiring as the Open Gauges wind, pressure and temperature gauges.
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

// ===========================================================================
// WI-FI / MQTT
// ===========================================================================
const char* ssid        = "YOUR_WIFI_NAME";
const char* password    = "YOUR_WIFI_PASSWORD";

const char* mqtt_server = "mqtt.cetools.org";
const int   mqtt_port   = 1883;

// The weeWX loop packet (JSON). The sketch looks for dayRain_mm, then dayRain_cm,
// dayRain_in, then a bare dayRain / day_rain key, and converts to mm automatically.
// A topic that carries a plain number (e.g. ".../dayRain_mm") also works — mm, unless
// the topic name ends in "_cm" or "_in".
const char* rain_topic = "personal/ucfnaps/downhamweather/loop";   // edit for your station

// Units of a bare "dayRain" / "day_rain" key (no unit suffix): "mm", "cm" or "in".
// weeWX in US units sends inches; METRICWX sends mm.
const char* bareKeyUnits = "mm";

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
// Breakpoints of the stretched scale: rainfall (mm) -> bearing (degrees clockwise
// from 12 o'clock). Taken from the dial's tick marks in the Fusion model.
const int   SCALE_POINTS = 4;
const float scaleMm[SCALE_POINTS]  = {    0.0,      2.0,     10.0,  50.0 };
const float scaleDeg[SCALE_POINTS] = { -135.0, -94.0909, -28.6364, 135.0 };
const float dialMaxMm = 50.0;
const float dialTopMm = 17.0;        // value at 12 o'clock

// How far the pointer must travel FORWARD (clockwise) from the magnet trigger
// point to sit exactly on the 12 o'clock (17 mm) mark. Same idea as the wind gauge:
//   stops short (anticlockwise of 17) -> increase    overshoots -> decrease
const float homeOffsetDeg = 13.0;

// BACKLASH: the 28BYJ-48 gearbox has a few degrees of play. Homing approaches
// clockwise, so clockwise moves land accurately but anticlockwise moves stop short.
// Fix: every anticlockwise move overshoots by this much, then comes back clockwise,
// so the pointer always arrives from the same side.
//   Still short after anticlockwise moves -> increase   (typical 2-6 deg)
const float backlashDeg = 4.0;

// Don't nudge the pointer for changes smaller than this (a Davis bucket tip is 0.2 mm)
const float moveDeadbandMm = 0.1;

// ===========================================================================
// LEDS
// ===========================================================================
#define NUM_LEDS        84
#define LED_BRIGHTNESS  50           // 0-255; turn it down if the board resets
const uint8_t backlightRGB[3] = {255, 255, 255};

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
float currentMm = NAN;      // latest reading
float shownMm   = NAN;      // value the pointer was last sent to
bool  coilsOn   = true;
unsigned long lastRehomeMs = 0;

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

// Bearing on the dial for a rainfall amount (piecewise linear between breakpoints)
float mmToDeg(float mm) {
  mm = constrain(mm, scaleMm[0], scaleMm[SCALE_POINTS - 1]);
  for (int i = 1; i < SCALE_POINTS; i++) {
    if (mm <= scaleMm[i]) {
      float f = (mm - scaleMm[i - 1]) / (scaleMm[i] - scaleMm[i - 1]);
      return scaleDeg[i - 1] + f * (scaleDeg[i] - scaleDeg[i - 1]);
    }
  }
  return scaleDeg[SCALE_POINTS - 1];
}

long mmToSteps(float mm) {
  return lround(mmToDeg(mm) * stepsPerDegree);                      // 0 = 12 o'clock
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
  runToPosition(0);          // park at 12 o'clock
  shownMm = NAN;             // force a fresh move to the current reading
}

// ===========================================================================
// LEDS
// ===========================================================================
void showBacklight() {
  const float k = LED_BRIGHTNESS / 255.0f;
  for (int i = 0; i < NUM_LEDS; i++)
    strip.setPixelColor(i, strip.Color(backlightRGB[0] * k, backlightRGB[1] * k, backlightRGB[2] * k));
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
// MQTT
// ===========================================================================
float readNumber(JsonVariant v) {
  if (v.is<const char*>()) return atof(v.as<const char*>());
  if (v.is<float>() || v.is<long>()) return v.as<float>();
  return NAN;
}

// Convert rainfall in the given units ("mm", "cm", "in") to mm
float toMm(float v, const char* units) {
  if (strstr(units, "cm")) return v * 10.0f;
  if (strstr(units, "in")) return v * 25.4f;
  return v;                                            // mm
}

bool endsWith(const char* s, const char* suffix) {
  size_t ls = strlen(s), lf = strlen(suffix);
  return ls >= lf && strcmp(s + ls - lf, suffix) == 0;
}

float extractRainMm(const char* topic, byte* payload, unsigned int length) {
  // Plain number payload — units taken from the topic name
  unsigned int i = 0;
  while (i < length && isspace(payload[i])) i++;
  if (i >= length) return NAN;
  if (payload[i] != '{') {
    char buf[24]; unsigned int n = min(length - i, (unsigned int)sizeof(buf) - 1);
    memcpy(buf, payload + i, n); buf[n] = 0;
    if (!isdigit(buf[0]) && buf[0] != '.') return NAN;   // "None", "null"
    float v = atof(buf);
    if (endsWith(topic, "_cm")) return toMm(v, "cm");
    if (endsWith(topic, "_in")) return toMm(v, "in");
    return v;
  }

  // JSON loop packet — keys in order of preference
  JsonDocument doc;
  if (deserializeJson(doc, (const char*)payload, length)) return NAN;
  const char* keys[] = { "dayRain_mm", "dayRain_cm", "dayRain_in", "dayRain", "day_rain" };
  for (const char* key : keys) {
    JsonVariant v = doc[key];
    if (v.isNull()) continue;
    float val = readNumber(v);
    if (isnan(val)) continue;
    const char* u = strstr(key, "_mm") ? "mm" : strstr(key, "_cm") ? "cm" :
                    strstr(key, "_in") ? "in" : bareKeyUnits;
    return toMm(val, u);
  }
  return NAN;
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  float mm = extractRainMm(topic, payload, length);
  if (isnan(mm) || mm < 0 || mm > 1000) return;     // ignore junk
  currentMm = mm;

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 60000) {
    lastPrint = millis();
    Serial.printf("Rain today %.1f mm\n", currentMm);
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
    String id = "OpenGauge-Rain-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    Serial.print("Connecting to MQTT... ");
    if (client.connect(id.c_str())) {
      Serial.println("connected");
      client.subscribe(rain_topic);
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
  strip.setBrightness(255);            // full; backlight is dimmed per-pixel in showBacklight()
  STATUS_STARTED;
  delay(1500);
  Serial.println("\nOpen Gauges — Daily Rainfall");

  pinMode(HALL_PWR, OUTPUT);
  hallPower(false);                    // sensor off until it's needed

  // Wi-Fi first, before the motor runs, so the two big power draws never overlap
  STATUS_WIFI;
  connectWiFi();

  // Home, calibrate, park at 12 o'clock (17 mm)
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
  Serial.println("Self-test: 0 -> 50 -> 0 mm");
  STATUS_SELFTEST;
  runToPosition(mmToSteps(0));          delay(1000);
  runToPosition(mmToSteps(dialMaxMm));  delay(1000);
  runToPosition(mmToSteps(0));

  client.setServer(mqtt_server, mqtt_port);
  client.setBufferSize(4096);          // weeWX loop packets are larger than the 256-byte default
  client.setCallback(mqttCallback);

  showBacklight();                     // normal white backlight = ready
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

  // Move the pointer if the reading has changed enough
  if (!isnan(currentMm) && (isnan(shownMm) || fabsf(currentMm - shownMm) >= moveDeadbandMm)) {
    powerCoils();
    moveToSteps(mmToSteps(currentMm));
    shownMm = currentMm;
  }

  stepper.run();
}
