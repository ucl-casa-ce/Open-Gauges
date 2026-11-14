/*
  Open Gauges — Wind Direction Gauge Lights (Arduino Nano 33 IoT)
  - Backlight mode: always on, always off, or on at dusk until an off time (midnight
    by default). Dusk is worked out on board from your latitude / longitude, and the
    time comes from the Wi-Fi module (WiFiNINA), with UK summer time applied.
*/
#include <AccelStepper.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <time.h>

// Define motor control pins for 28BYJ-48
#define motorPin1  9
#define motorPin2  10
#define motorPin3  11
#define motorPin4  12

// Define hall sensor pin (magnet near North for homing)
#define hallSensorPin 8

// NeoPixel configuration
#define neoPin 7                      // Pin where the NeoPixel is connected
#define neoNumPixels 80               // Number of NeoPixels
#define neoBrightness    120           // Brightness level (0-255)
#define neoType NEO_GRB + NEO_KHZ800  // NeoPixel strip type (e.g., NEO_GRB)

// Create a NeoPixel object
Adafruit_NeoPixel strip = Adafruit_NeoPixel(neoNumPixels, neoPin, neoType);

// Wi-Fi credentials
const char* ssid     = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

// MQTT broker details
const char* mqtt_server = "mqtt.cetools.org";
const int   mqtt_port   = 1883;
const char* wind_topic  = "personal/ucfnaps/downhamweather/windDir";

// ---------------------------------------------------------------------------
// NORTH OFFSET (degrees)
// The hall sensor triggers slightly BEFORE true North because of where the
// magnet sits. This is how far the pointer must travel FORWARD (clockwise)
// from the magnet trigger point to land exactly on the N mark.
//   Pointer stops short of N (anticlockwise of it)  -> increase this value
//   Pointer overshoots N (clockwise of it)           -> decrease this value
// Negative values are fine if the magnet triggers after North.
// ---------------------------------------------------------------------------
const float northOffsetDeg = 6.0;

// Define stepper parameters
// These are RUNTIME variables, not fixed defines: a full magnet->magnet sweep at
// startup measures the real step count, so any motor (2048, 4096, etc.) self-calibrates.
long  stepsPerRevolution = 2048;                        // Default; overwritten by calibration
float stepsPerDegree     = stepsPerRevolution / 360.0;  // Recomputed after calibration

// Auto re-home: periodically re-zero on the North magnet to cancel any drift.
const unsigned long rehomeIntervalMs = 6UL * 60UL * 60UL * 1000UL; // every 6 hours
unsigned long lastRehomeMs = 0;

// Homing / calibration rotation speed (steps/sec). The 28BYJ-48 stays reliable up
// to ~500; back this off if it stalls or misses the magnet.
const float homingSpeed = 300.0;

// Create AccelStepper instance
AccelStepper stepper(AccelStepper::HALF4WIRE, motorPin1, motorPin3, motorPin2, motorPin4);

// Switch the motor coils off once the pointer has arrived: the motor and ULN2003
// stay cool and stop drawing current, and the gearbox holds the pointer in place.
const bool releaseCoilsWhenIdle = true;
bool coilsOn = true;

void powerCoils() {
  if (!coilsOn) { stepper.enableOutputs(); coilsOn = true; }
}

// Wi-Fi and MQTT clients
WiFiClient espClient;
PubSubClient client(espClient);

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

// Local time: standard offset from UTC, plus summer time on the UK / EU rule
// (+1 h from 01:00 UTC on the last Sunday of March to the last Sunday of October)
const int  utcOffsetMinutes = 0;      // UK = 0
const bool useSummerTime    = true;

bool backlightOn = true;          // set by updateBacklightState()
unsigned long syncEpoch = 0, syncMillis = 0;   // clock from WiFi.getTime()

// Global variable to store the target wind direction (degrees, 0-360)
float targetWindDir = 0.0;
float lastTargetWindDir = -1.0; // To track changes in wind direction

void printWiFiStatus() {
  switch (WiFi.status()) {
    case WL_IDLE_STATUS:     Serial.println("Wi-Fi status: Idle"); break;
    case WL_NO_SSID_AVAIL:   Serial.println("Wi-Fi status: SSID not available"); break;
    case WL_CONNECT_FAILED:  Serial.println("Wi-Fi status: Connection failed"); break;
    case WL_DISCONNECTED:    Serial.println("Wi-Fi status: Disconnected"); break;
    case WL_CONNECTED:       Serial.println("Wi-Fi status: Connected"); break;
    default:                 Serial.println("Wi-Fi status: Unknown"); break;
  }
}

void connectToWiFi() {
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    printWiFiStatus();
    retryCount++;

    if (retryCount > 30) { // Timeout after 30 seconds
      Serial.println("\nWi-Fi connection failed! Restarting...");
      NVIC_SystemReset();
    }
  }

  Serial.println("\nWi-Fi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnectToMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT broker...");

    if (client.connect("NanoIoTStepperClient")) {
      Serial.println("connected.");
      client.subscribe(wind_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Raw MQTT message: ");
  Serial.println(message);

  if (String(topic) == wind_topic) {
    targetWindDir = message.toFloat();
    Serial.print("Updated target wind direction (deg): ");
    Serial.println(targetWindDir);
  }
}

void moveStepperToWindDirection() {
  if (targetWindDir == lastTargetWindDir) {
    return;
  }

  // Normalise incoming direction to 0-360
  float dir = fmod(targetWindDir, 360.0);
  if (dir < 0) dir += 360.0;

  // Step 0 is now TRUE North (offset already applied when homing)
  long targetStep = lround(dir * stepsPerDegree);

  // --- SHORTEST-PATH ROTATION ---
  long current = stepper.currentPosition();
  long currentMod = ((current % stepsPerRevolution) + stepsPerRevolution) % stepsPerRevolution;

  long delta = targetStep - currentMod;
  if (delta >  stepsPerRevolution / 2) delta -= stepsPerRevolution;
  if (delta < -stepsPerRevolution / 2) delta += stepsPerRevolution;
  // ------------------------------

  Serial.print("Setting new target position (steps): ");
  Serial.println(current + delta);

  powerCoils();
  stepper.moveTo(current + delta);
  lastTargetWindDir = targetWindDir;
}

// True while the magnet is under the hall sensor.
// If yours reads HIGH on the magnet, change to: return digitalRead(hallSensorPin) == HIGH;
bool atNorth() {
  return digitalRead(hallSensorPin) == LOW;
}

// The magnet trigger point sits northOffsetDeg BEFORE true North, so we label it
// with a negative position. Step 0 is then exactly on the N mark.
void applyNorthOffset() {
  stepper.setCurrentPosition(-lround(northOffsetDeg * stepsPerDegree));
}

// Rotate forward until the LEADING edge of the magnet is found, then set the
// coordinate system so that 0 = true North.
void seekNorthAndZero() {
  powerCoils();
  stepper.setMaxSpeed(homingSpeed);
  stepper.setAcceleration(200);
  stepper.setSpeed(homingSpeed);     // forward

  while (atNorth()) stepper.runSpeed();   // step off the magnet if we start on it
  while (!atNorth()) stepper.runSpeed();  // roll on until the magnet is found

  applyNorthOffset();
}

// Drive from the magnet trigger point to true North (position 0), blocking.
void moveToTrueNorth() {
  stepper.setMaxSpeed(homingSpeed);
  stepper.setAcceleration(200);
  stepper.moveTo(0);
  while (stepper.distanceToGo() != 0) stepper.run();
}

void homeStepper() {
  Serial.println("Homing stepper motor to North...");
  seekNorthAndZero();
  Serial.println("Homing complete (on magnet edge).");
}

// Measure one full revolution between two identical leading edges, rotating in
// a single direction throughout. Sets stepsPerRevolution/stepsPerDegree.
void calibrateStepsPerRevolution() {
  Serial.println("Calibrating: measuring one magnet -> magnet revolution...");
  stepper.setMaxSpeed(homingSpeed);
  stepper.setAcceleration(200);

  // We're parked ON the magnet's leading edge, so start the count here.
  // setCurrentPosition() resets speed to 0, so setSpeed() must come after it.
  stepper.setCurrentPosition(0);
  stepper.setSpeed(homingSpeed);

  while (atNorth()) stepper.runSpeed();     // step off the magnet
  while (!atNorth()) stepper.runSpeed();    // next leading edge = one full turn

  long measured = labs(stepper.currentPosition());
  if (measured > 100) {
    stepsPerRevolution = measured;
    stepsPerDegree = (float)stepsPerRevolution / 360.0;
  } else {
    Serial.println("Calibration looked wrong; keeping default step count.");
  }

  // We finished on the magnet edge: label it with the offset so 0 = true North
  applyNorthOffset();

  Serial.print("Steps per revolution: ");
  Serial.println(stepsPerRevolution);
  Serial.print("Steps per degree: ");
  Serial.println(stepsPerDegree, 4);
  Serial.print("North offset (steps): ");
  Serial.println(lround(northOffsetDeg * stepsPerDegree));
}

// Quick re-zero on the magnet, then re-apply the current wind direction.
void rehome() {
  Serial.println("Auto re-home to North...");
  seekNorthAndZero();
  stepper.setMaxSpeed(400);
  stepper.setAcceleration(80);
  lastTargetWindDir = -1.0;   // force a fresh move to the latest direction
  Serial.println("Re-home complete.");
}

// ---- Clock ----------------------------------------------------------------
// Ask the Wi-Fi module for the time (it gets it by NTP once connected)
void syncClock() {
  for (int i = 0; i < 10; i++) {
    unsigned long t = WiFi.getTime();
    if (t > 1700000000UL) {
      syncEpoch = t; syncMillis = millis();
      Serial.print("Clock set, UTC epoch "); Serial.println(t);
      return;
    }
    delay(1000);
  }
  Serial.println("Couldn't get the time yet; will try again.");
}

time_t nowUtc() {
  if (!syncEpoch) return 0;
  return (time_t)(syncEpoch + (millis() - syncMillis) / 1000);
}

// Day of week, 0 = Sunday
int dayOfWeek(int y, int m, int d) {
  static const int k[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y--;
  return (y + y / 4 - y / 100 + y / 400 + k[m - 1] + d) % 7;
}

bool isSummerTime(time_t utc) {
  if (!useSummerTime) return false;
  struct tm u; gmtime_r(&utc, &u);
  int y = u.tm_year + 1900, m = u.tm_mon + 1, d = u.tm_mday;
  if (m < 3 || m > 10) return false;
  if (m > 3 && m < 10) return true;
  int lastSun = 31 - dayOfWeek(y, m, 31);           // March and October both have 31 days
  if (m == 3) return d > lastSun || (d == lastSun && u.tm_hour >= 1);
  return d < lastSun || (d == lastSun && u.tm_hour < 1);
}

// ---- Backlight schedule --------------------------------------------------
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

void setBacklight(bool on) {
  for (int i = 0; i < neoNumPixels; i++)
    strip.setPixelColor(i, on ? strip.Color(255, 255, 255) : 0);
  strip.show();
}

void updateBacklightState() {
  bool on = true;
  if (lightsMode == LIGHTS_OFF) {
    on = false;
  } else if (lightsMode == LIGHTS_DUSK) {
    time_t utcNow = nowUtc();
    if (utcNow > 1700000000) {                     // until the clock is set, stay on
      time_t localNow = utcNow + (utcOffsetMinutes + (isSummerTime(utcNow) ? 60 : 0)) * 60L;
      struct tm local, utc;
      gmtime_r(&localNow, &local);
      gmtime_r(&utcNow, &utc);
      int nowMin  = local.tm_hour * 60 + local.tm_min;
      int offMin  = lightsOffHour * 60 + lightsOffMinute;
      int duskMin = duskMinutesLocal(local, utc);
      if (duskMin < 0)            on = false;                         // no dusk today
      else if (offMin <= duskMin) on = (nowMin >= duskMin) || (nowMin < offMin);   // off after midnight
      else                        on = (nowMin >= duskMin) && (nowMin < offMin);   // off the same evening

      static int lastLoggedDay = -1;
      if (local.tm_yday != lastLoggedDay) {
        lastLoggedDay = local.tm_yday;
        char buf[48];
        snprintf(buf, sizeof(buf), "Dusk today %02d:%02d, lights off %02d:%02d",
                 duskMin / 60, duskMin % 60, lightsOffHour, lightsOffMinute);
        Serial.println(buf);
      }
    }
  }
  if (on != backlightOn) {
    backlightOn = on;
    setBacklight(on);
    Serial.println(on ? "Backlight on" : "Backlight off");
  }
}

void initializeNeoPixel() {
  strip.begin();
  strip.setBrightness(neoBrightness);
  for (int i = 0; i < neoNumPixels; i++) {
    strip.setPixelColor(i, strip.Color(255, 255, 255));
  }
  strip.show();
  Serial.println("NeoPixel initialized and set to white.");
}

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing system...");

  pinMode(hallSensorPin, INPUT_PULLUP);

  initializeNeoPixel();

  // Find the magnet edge
  homeStepper();

  Serial.println("On magnet. Waiting 5 seconds before calibration...");
  delay(5000);

  // Measure the real steps-per-revolution (ends on the magnet edge, offset applied)
  calibrateStepsPerRevolution();
  lastRehomeMs = millis();

  // Now nudge forward from the magnet onto TRUE North
  Serial.println("Moving to true North...");
  moveToTrueNorth();
  Serial.println("At true North. Check the pointer against the N mark.");

  // --- GLOBAL EASING CONFIGURATION ---
  stepper.setMaxSpeed(400);
  stepper.setAcceleration(80);
  // -----------------------------------

  delay(5000);

  // Self-test: sweep to South (180 deg) then back to North (0 deg)
  Serial.println("Test: moving to South (180 deg)...");
  stepper.moveTo(lround(180.0 * stepsPerDegree));
  while (stepper.distanceToGo() != 0) stepper.run();
  Serial.println("Reached South.");

  delay(5000);

  Serial.println("Returning to North (0 deg)...");
  stepper.moveTo(0);
  while (stepper.distanceToGo() != 0) stepper.run();
  Serial.println("Returned to North.");

  connectToWiFi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);

  syncClock();
  updateBacklightState();
  setBacklight(backlightOn);

  Serial.println("System ready.");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi connection lost. Reconnecting...");
    connectToWiFi();
  }

  if (!client.connected()) {
    reconnectToMQTT();
  }
  client.loop();

  // Periodic auto re-home: only when idle so we don't interrupt a move
  if (millis() - lastRehomeMs > rehomeIntervalMs && stepper.distanceToGo() == 0) {
    rehome();
    lastRehomeMs = millis();
  }

  // Backlight schedule: check every 30 s; re-read the clock every 6 h (or until it's set)
  static unsigned long lastLightsCheck = 0, lastClockSync = 0;
  if (stepper.distanceToGo() == 0 && millis() - lastLightsCheck > 30000) {
    lastLightsCheck = millis();
    if (!syncEpoch || millis() - lastClockSync > 6UL * 60UL * 60UL * 1000UL) {
      lastClockSync = millis();
      syncClock();
    }
    updateBacklightState();
  }

  moveStepperToWindDirection();
  stepper.run();

  // Pointer has arrived: release the coils until the next move
  if (releaseCoilsWhenIdle && coilsOn && stepper.distanceToGo() == 0) {
    stepper.disableOutputs();
    coilsOn = false;
  }
}
