#include <AccelStepper.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <avr/wdt.h>

// Define motor control pins for 28BYJ-48
#define motorPin1  7
#define motorPin2  8
#define motorPin3  9
#define motorPin4  10

// Define limit switch pin
#define limitSwitchPin 11

// NeoPixel configuration
#define neoPin 6                      // Pin where the NeoPixel is connected
#define neoNumPixels 33               // Number of NeoPixels
#define neoBrightness 100             // Brightness level (0-255)
#define neoType NEO_GRB + NEO_KHZ800  // NeoPixel strip type (e.g., NEO_GRB)

// Create a NeoPixel object
Adafruit_NeoPixel strip = Adafruit_NeoPixel(neoNumPixels, neoPin, neoType);

// Wi-Fi credentials
const char* ssid = "Your Wifi";
const char* password = "Your Wifi";

// MQTT broker details
const char* mqtt_server = "mqtt.cetools.org"; // Our Open MQTT Broker - Edit Accordingly
const int mqtt_port = 1883;
const char* wind_topic = "personal/ucfnaps/downhamweather/windSpeed_mph"; // MQTT Topic 

// Define stepper parameters
#define stepsPerRevolution 2048    // Steps per revolution of the output shaft
const float gearCircumference = 1; // Circumference of the gear in cm
const float maxDistanceCm = 1.53;  // Maximum distance the stepper should move (linked to max speed)

// Calculate steps per cm
const float stepsPerCm = stepsPerRevolution / gearCircumference;

// Create AccelStepper instance
AccelStepper stepper(AccelStepper::HALF4WIRE, motorPin1, motorPin3, motorPin2, motorPin4);

// Wi-Fi and MQTT clients
WiFiClient espClient;
PubSubClient client(espClient);

// Global variable to store the target wind speed
float targetWindSpeed = 0.0;
float lastTargetWindSpeed = -1.0; // To track changes in wind speed

void printWiFiStatus() {
  switch (WiFi.status()) {
    case WL_IDLE_STATUS:
      Serial.println("Wi-Fi status: Idle");
      break;
    case WL_NO_SSID_AVAIL:
      Serial.println("Wi-Fi status: SSID not available");
      break;
    case WL_CONNECT_FAILED:
      Serial.println("Wi-Fi status: Connection failed");
      break;
    case WL_DISCONNECTED:
      Serial.println("Wi-Fi status: Disconnected");
      break;
    case WL_CONNECTED:
      Serial.println("Wi-Fi status: Connected");
      break;
    default:
      Serial.println("Wi-Fi status: Unknown");
      break;
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
    printWiFiStatus(); // Debugging Wi-Fi connection status
    retryCount++;

    if (retryCount > 30) { // Timeout after 30 seconds
      Serial.println("\nWi-Fi connection failed! Restarting...");
      wdt_enable(WDTO_15MS); // Trigger a watchdog reset
      while (true);          // Wait for the watchdog to reset the board
    }
  }

  Serial.println("\nWi-Fi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnectToMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT broker...");

    if (client.connect("NanoAVRStepperClient")) {
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

  // Update wind speed if the correct topic is received
  if (String(topic) == wind_topic) {
    targetWindSpeed = message.toFloat();
    Serial.print("Updated target wind speed: ");
    Serial.println(targetWindSpeed);
  }
}

void moveStepperToWindSpeed() {
  // Skip if wind speed hasn't changed
  if (targetWindSpeed == lastTargetWindSpeed) {
    return;
  }

  // Scale wind speed (0–60 mph) to distance (0–maxDistanceCm)
  float targetDistance = (targetWindSpeed / 60.0) * maxDistanceCm;

  // Convert distance to steps
  long targetPositionSteps = targetDistance * stepsPerCm;

  Serial.print("Moving stepper to distance (cm): ");
  Serial.println(targetDistance);

  // Move the stepper to the target position
  stepper.moveTo(targetPositionSteps);

  while (stepper.distanceToGo() != 0) {
    stepper.run();
  }

  Serial.print("Stepper moved to position (cm): ");
  Serial.println(targetDistance);

  // Update last target speed
  lastTargetWindSpeed = targetWindSpeed;
}

void homeStepper() {
  Serial.println("Homing stepper motor...");

  // Move stepper motor backward until the limit switch is triggered
  stepper.setMaxSpeed(300);    // Max speed in steps per second
  stepper.setAcceleration(100); // Acceleration in steps per second^2
  stepper.setSpeed(-100);      // Move backward

  while (digitalRead(limitSwitchPin) == HIGH) { // Assuming HIGH means not triggered
    stepper.runSpeed();
  }

  stepper.stop(); // Stop the motor
  stepper.setCurrentPosition(0); // Set home position to 0
  Serial.println("Homing complete. Stepper position set to 0.");
}

void initializeNeoPixel() {
  strip.begin();
  strip.setBrightness(neoBrightness);

  // Set all pixels to white
  for (int i = 0; i < neoNumPixels; i++) {
    strip.setPixelColor(i, strip.Color(255, 255, 255)); // White color
  }

  strip.show();
  Serial.println("NeoPixel initialized and set to white.");
}

void setup() {
  Serial.begin(115200);

  Serial.println("Initializing system...");

  // Configure limit switch
  pinMode(limitSwitchPin, INPUT_PULLUP); // Assuming a normally open (NO) switch

  // Initialize NeoPixel
  initializeNeoPixel();

  // Home the stepper motor
  homeStepper();

  // Initialize Wi-Fi and MQTT
  connectToWiFi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);

  Serial.println("System ready.");
}

void loop() {
  // Ensure Wi-Fi is connected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi connection lost. Reconnecting...");
    connectToWiFi();
  }

  // Ensure MQTT is connected
  if (!client.connected()) {
    reconnectToMQTT();
  }
  client.loop();

  // Adjust stepper based on received wind speed
  moveStepperToWindSpeed();

  // Add a small delay to avoid excessive stepper updates
  delay(1000);
}