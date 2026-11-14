/*
  Open Gauges — Wind Direction Gauge Lights
  Board: ESP32-C3 Super Mini (Arduino core for ESP32 v3.x)
  Motor: 28BYJ-48 + ULN2003 driver   Homing: KY-003 hall sensor + 5 mm magnet
  LEDs:  WS2812 / NeoPixel strip around the rim (white backlight)

  - Reads wind direction (degrees) from MQTT: a plain number (e.g. ".../windDir" -> 247)
    or a weeWX loop packet (JSON, "windDir" key). Calm readings (no direction) are ignored.
  - Full 360° dial: the pointer always takes the shortest way round.
  - Backlight mode: always on, always off, or on at dusk until an off time (midnight
    by default). Dusk is worked out on board from your latitude / longitude.
  - Start-up progress on the ring: RED started, BLUE Wi-Fi, YELLOW homing,
    PURPLE self-test, then the backlight (or dark) when ready.
  - Same wiring as the Open Gauges pressure and temperature gauges.
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
#include <time.h>

// ===========================================================================
// WI-FI / MQTT
// ===========================================================================
const char* ssid        = "YOUR_WIFI_NAME";
const char* password    = "YOUR_WIFI_PASSWORD";

const char* mqtt_server = "mqtt.cetools.org";
const int   mqtt_port   = 1883;

// A topic carrying plain degrees (e.g. 247), or a weeWX loop packet (JSON) with "windDir".
const char* wind_topic  = "personal/ucfnaps/downhamweather/windDir";   // edit for your station

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
// DIAL
// ===========================================================================
// How far the pointer must travel FORWARD (clockwise) from the magnet trigger
// point to sit exactly on the N mark.
//   Pointer stops short of N (anticlockwise of it) -> increase
//   Pointer overshoots N (clockwise of it)          -> decrease
// Negative values are fine if the magnet triggers after North.
const float northOffsetDeg = 6.0;

// BACKLASH: the 28BYJ-48 gearbox has a few degrees of play. Homing approaches
// clockwise, so clockwise moves land accurately but anticlockwise moves stop short.
// Fix: every anticlockwise move overshoots by this much, then comes back clockwise,
// so the pointer always arrives from the same side.
//   Still short after anticlockwise moves -> increase   (typical 2-6 deg)
const float backlashDeg = 4.0;

// Don't nudge the pointer for changes smaller than this (degrees)
const float moveDeadbandDeg = 2.0;

// ===========================================================================
// LEDS
// ===========================================================================
#define NUM_LEDS        84
#define LED_BRIGHTNESS  120          // 0-255; turn it down if the board resets
const uint8_t backlightRGB[3] = {255, 255, 255};

// ===========================================================================
// BACKLIGHT MODE
// ===========================================================================
//   LIGHTS_ON   = backlight always on
//   LIGHTS_OFF  = backlight always off
//   LIGHTS_DUSK = on at dusk, off at lightsOffHour:lightsOffMinute
enum LightsMode { LIGHTS_ON, LIGHTS_OFF, LIGHTS_DUSK };
const LightsMode lightsMode = LIGHTS_DUSK;

const int   lightsOffHour   = 0;      // off time, local (0:00 = midnight; 23 / 30 = 23:30)
const int   lightsOffMinute = 0;

// Where the gauge is, for working out dusk (decimal degrees, east positive)
const float latitude  = 0.0;          // YOUR latitude, e.g. 51.51 for London
const float longitude = 0.0;          // YOUR longitude, e.g. -0.13 for London (west is negative)

// Sun angle that counts as dusk: -0.833 = sunset, -6 = civil dusk (~35 min later in the UK)
const float duskSunAngle = -0.833;

// Local time zone (UK: GMT / BST)
const char* tzInfo = "GMT0BST,M3.5.0/1,M10.5.0";

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
float currentDir = NAN;     // latest wind direction (0-360)
float shownDir   = NAN;     // direction the pointer was last sent to
bool  coilsOn    = true;
unsigned long lastRehomeMs = 0;
bool  backlightOn = true;   // set by updateBacklightState()
bool  ledsDirty   = true;

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

float normDeg(float d) {
  d = fmodf(d, 360.0f);
  return d < 0 ? d + 360.0f : d;
}

// Angular difference a-b, in -180..180
float angleDiff(float a, float b) {
  float d = normDeg(a - b);
  return d > 180.0f ? d - 360.0f : d;
}

// Absolute step target for a bearing, taking the shortest way round from where we are
long dirToSteps(float dir) {
  long target = lround(normDeg(dir) * stepsPerDegree);
  long cur    = stepper.currentPosition();
  long curMod = ((cur % stepsPerRevolution) + stepsPerRevolution) % stepsPerRevolution;
  long delta  = target - curMod;
  if (delta >  stepsPerRevolution / 2) delta -= stepsPerRevolution;
  if (delta < -stepsPerRevolution / 2) delta += stepsPerRevolution;
  return cur + delta;
}

void applyNorthOffset() {
  stepper.setCurrentPosition(-lround(northOffsetDeg * stepsPerDegree));
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
  applyNorthOffset();
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
  applyNorthOffset();
  Serial.printf("Steps/rev: %ld  steps/deg: %.4f\n", stepsPerRevolution, stepsPerDegree);
}

void rehome() {
  Serial.println("Auto re-home...");
  seekMagnetAndZero();
  hallPower(false);
  stepper.setMaxSpeed(runSpeed);
  stepper.setAcceleration(runAccel);
  runToPosition(0);          // park at North
  shownDir = NAN;            // force a fresh move to the current reading
}

// ===========================================================================
// LEDS
// ===========================================================================
void showBacklight() {
  const float k = backlightOn ? LED_BRIGHTNESS / 255.0f : 0.0f;
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
// BACKLIGHT SCHEDULE
// ===========================================================================
// Today's dusk as minutes after local midnight. Standard sunrise/sunset equation
// (accurate to a minute or two). Returns -1 if the sun doesn't go that low today
// (light all night in the far north), 0 if it never comes up.
int duskMinutesLocal(const struct tm& local, const struct tm& utc) {
  const float D = PI / 180.0f;
  int   N       = local.tm_yday + 1;
  float lngHour = longitude / 15.0f;
  float t = N + ((18.0f - lngHour) / 24.0f);                    // evening
  float M = 0.9856f * t - 3.289f;
  float L = fmodf(M + 1.916f * sinf(M * D) + 0.020f * sinf(2 * M * D) + 282.634f + 360.0f, 360.0f);
  float RA = fmodf(atanf(0.91764f * tanf(L * D)) / D + 360.0f, 360.0f);
  RA += floorf(L / 90.0f) * 90.0f - floorf(RA / 90.0f) * 90.0f;  // same quadrant as L
  RA /= 15.0f;
  float sinDec = 0.39782f * sinf(L * D);
  float cosDec = cosf(asinf(sinDec));
  float zenith = 90.0f - duskSunAngle;
  float cosH = (cosf(zenith * D) - sinDec * sinf(latitude * D)) / (cosDec * cosf(latitude * D));
  if (cosH < -1) return -1;                                       // never that dark
  if (cosH >  1) return 0;                                        // dark all day
  float H  = acosf(cosH) / D / 15.0f;
  float UT = fmodf(H + RA - 0.06571f * t - 6.622f - lngHour + 48.0f, 24.0f);

  // Local offset from UTC (handles GMT / BST)
  int offset = (local.tm_hour * 60 + local.tm_min) - (utc.tm_hour * 60 + utc.tm_min);
  if (offset >  720) offset -= 1440;
  if (offset < -720) offset += 1440;
  return ((int)lroundf(UT * 60.0f) + offset + 1440) % 1440;
}

void updateBacklightState() {
  bool on = true;
  if (lightsMode == LIGHTS_OFF) {
    on = false;
  } else if (lightsMode == LIGHTS_DUSK) {
    time_t now = time(nullptr);
    if (now > 1700000000) {                       // until the clock is set, stay on
      struct tm local, utc;
      localtime_r(&now, &local);
      gmtime_r(&now, &utc);
      int nowMin  = local.tm_hour * 60 + local.tm_min;
      int offMin  = lightsOffHour * 60 + lightsOffMinute;
      int duskMin = duskMinutesLocal(local, utc);
      if (duskMin < 0)          on = false;                       // no dusk today
      else if (offMin <= duskMin) on = (nowMin >= duskMin) || (nowMin < offMin);   // off after midnight
      else                      on = (nowMin >= duskMin) && (nowMin < offMin);     // off the same evening

      static int lastLoggedDay = -1;
      if (local.tm_yday != lastLoggedDay) {
        lastLoggedDay = local.tm_yday;
        Serial.printf("Dusk today %02d:%02d, lights off %02d:%02d\n",
                      duskMin / 60, duskMin % 60, lightsOffHour, lightsOffMinute);
      }
    }
  }
  if (on != backlightOn) {
    backlightOn = on;
    ledsDirty = true;
    Serial.printf("Backlight %s\n", on ? "on" : "off");
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

float extractDirection(byte* payload, unsigned int length) {
  unsigned int i = 0;
  while (i < length && isspace(payload[i])) i++;
  if (i >= length) return NAN;

  // Plain number payload
  if (payload[i] != '{') {
    char buf[24]; unsigned int n = min(length - i, (unsigned int)sizeof(buf) - 1);
    memcpy(buf, payload + i, n); buf[n] = 0;
    if (!isdigit(buf[0]) && buf[0] != '-' && buf[0] != '.') return NAN;   // "None", "null"
    return atof(buf);
  }

  // JSON loop packet
  JsonDocument doc;
  if (deserializeJson(doc, (const char*)payload, length)) return NAN;
  const char* keys[] = { "windDir", "wind_dir", "windDir_degree_compass", "winddir" };
  for (const char* key : keys) {
    JsonVariant v = doc[key];
    if (v.isNull()) continue;            // weeWX sends null when it's calm
    float val = readNumber(v);
    if (!isnan(val)) return val;
  }
  return NAN;
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  float dir = extractDirection(payload, length);
  if (isnan(dir) || dir < -360 || dir > 720) return;   // calm or junk: hold position
  currentDir = normDeg(dir);

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 60000) {
    lastPrint = millis();
    Serial.printf("Wind direction %.0f deg\n", currentDir);
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
    String id = "OpenGauge-Wind-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    Serial.print("Connecting to MQTT... ");
    if (client.connect(id.c_str())) {
      Serial.println("connected");
      client.subscribe(wind_topic);
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
  Serial.println("\nOpen Gauges — Wind Direction");

  pinMode(HALL_PWR, OUTPUT);
  hallPower(false);                    // sensor off until it's needed

  // Wi-Fi first, before the motor runs, so the two big power draws never overlap
  STATUS_WIFI;
  connectWiFi();

  // Home, calibrate, park at North
  STATUS_HOMING;
  seekMagnetAndZero();
  delay(1000);
  calibrateStepsPerRevolution();
  hallPower(false);                    // homing done — sensor off
  stepper.setMaxSpeed(runSpeed);
  stepper.setAcceleration(runAccel);
  runToPosition(0);
  lastRehomeMs = millis();
  Serial.println("At North. Check the pointer against the N mark.");
  delay(2000);

  // Self-test: North -> South -> North
  Serial.println("Self-test: N -> S -> N");
  STATUS_SELFTEST;
  runToPosition(dirToSteps(180));  delay(1000);
  runToPosition(dirToSteps(0));

  // Clock (for the dusk schedule)
  configTzTime(tzInfo, "pool.ntp.org", "time.google.com");
  for (int t = 0; t < 20 && time(nullptr) < 1700000000; t++) delay(500);

  client.setServer(mqtt_server, mqtt_port);
  client.setBufferSize(4096);          // weeWX loop packets are larger than the 256-byte default
  client.setCallback(mqttCallback);

  updateBacklightState();
  showBacklight();                     // backlight (or dark) = ready
  ledsDirty = false;
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

  static unsigned long lastLightsCheck = 0;
  if (millis() - lastLightsCheck > 30000) { lastLightsCheck = millis(); updateBacklightState(); }
  if (ledsDirty) { ledsDirty = false; showBacklight(); }

  // Move the pointer if the reading has changed enough
  if (!isnan(currentDir) && (isnan(shownDir) || fabsf(angleDiff(currentDir, shownDir)) >= moveDeadbandDeg)) {
    powerCoils();
    moveToSteps(dirToSteps(currentDir));
    shownDir = currentDir;
  }

  stepper.run();
}
