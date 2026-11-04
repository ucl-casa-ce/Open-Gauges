#include <AccelStepper.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>

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
#define neoBrightness    255           // Brightness level (0-255)
#define neoType NEO_GRB + NEO_KHZ800  // NeoPixel strip type (e.g., NEO_GRB)

// Create a NeoPixel object
Adafruit_NeoPixel strip = Adafruit_NeoPixel(neoNumPixels, neoPin, neoType);

// Wi-Fi credentials
const char* ssid     = "Your WIFI SSID";
const char* password = "Your Password";

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

// Wi-Fi and MQTT clients
WiFiClient espClient;
PubSubClient client(espClient);

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

  moveStepperToWindDirection();
  stepper.run();
}
