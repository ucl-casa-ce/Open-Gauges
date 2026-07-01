#include <AccelStepper.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>

// Define motor control pins for 28BYJ-48
#define motorPin1  9
#define motorPin2  10
#define motorPin3  11
#define motorPin4  12

// Define limit switch pin
#define limitSwitchPin 8

// NeoPixel configuration
#define neoPin 7                      // Pin where the NeoPixel is connected
#define neoNumPixels 80               // Number of NeoPixels
#define neoBrightness    30          // Brightness level (0-255)
#define neoType NEO_GRB + NEO_KHZ800  // NeoPixel strip type (e.g., NEO_GRB)

// Create a NeoPixel object
Adafruit_NeoPixel strip = Adafruit_NeoPixel(neoNumPixels, neoPin, neoType);

// Wi-Fi credentials
const char* ssid = "";
const char* password = "";

// MQTT broker details
const char* mqtt_server = "mqtt.cetools.org";
const int mqtt_port = 1883;
const char* wind_topic = "personal/ucfnaps/downhamweather/windSpeed_mph";

// Define stepper parameters
#define stepsPerRevolution 2048    // Steps per revolution of the output shaft
const float gearCircumference = 1; // Circumference of the gear in cm
const float maxDistanceCm = 1.34;  // Maximum distance the stepper should move (linked to max speed)

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
      NVIC_SystemReset(); // Reset the board
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

  Serial.print("Setting new target position (steps): ");
  Serial.println(targetPositionSteps);

  // Set the new target (AccelStepper will calculate the easing curve automatically)
  stepper.moveTo(targetPositionSteps);

  // Update last target speed
  lastTargetWindSpeed = targetWindSpeed;
}

void homeStepper() {
  Serial.println("Homing stepper motor...");

  // Temporary slow settings just for safe homing
  stepper.setMaxSpeed(150);    
  stepper.setAcceleration(100); 
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
  
  // --- GLOBAL EASING CONFIGURATION ---
  // Lower max speed and much lower acceleration makes easing highly visible 
  // over tiny distances (like 1.34cm).
  stepper.setMaxSpeed(400);      // Dropped from 800 to keep movements smooth
  stepper.setAcceleration(80);   // Dropped from 300 to stretch out the ease-in/out ramp
  // -----------------------------------
  // Pause for 5 seconds
  delay(5000);

  // Move to the 60 mph position (Testing setup easing)
  Serial.println("Moving to 60 mph mark...");
  float targetDistance60 = (60.0 / 60.0) * maxDistanceCm;
  long targetPositionSteps60 = targetDistance60 * stepsPerCm;

  stepper.moveTo(targetPositionSteps60);
  while (stepper.distanceToGo() != 0) {
    stepper.run();
  }
  Serial.println("Reached 60 mph mark.");

  // Pause for 5 seconds
  delay(5000);

  // Move back to 0 position
  Serial.println("Returning to 0 mph mark...");
  stepper.moveTo(0);
  while (stepper.distanceToGo() != 0) {
    stepper.run();
  }
  Serial.println("Returned to 0 mph mark.");

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

  // Non-blocking checking of incoming target speeds
  moveStepperToWindSpeed();

  // CRITICAL: This must run continuously as fast as possible. 
  // It increments the stepper position to execute the easing curves.
  stepper.run();
}
