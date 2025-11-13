#include <WiFiNINA.h>
#include <PubSubClient.h>
#include <AccelStepperWithDistance.h>
#include <avr/wdt.h> // Watchdog timer for reset

// Add in your Wifi

// Wi-Fi credentials
const char* ssid = "";
const char* password = "";

// MQTT broker details  - an open data example is provided
const char* mqtt_server = "mqtt.cetools.org";
const int mqtt_port = 1883;
const char* wind_speed_topic = "personal/ucfnaps/downhamweather/windgust1min";

//const char* wind_speed_topic = "personal/ucfnaps/configure";

// Define motor control pins for 28BYJ-48
#define motorPin1  6
#define motorPin2  7
#define motorPin3  8
#define motorPin4  9

// Define limit switch pin
#define limitSwitchPin 10

// Direction reversal option
const bool reverseDirection = true; // Set to true to reverse motor direction

// Adjustable travel parameters
float travelDistanceCm = 97; // Adjust this parameter for different travel distances (in cm)
const float travelPerRevolution = 0.21; // Travel per revolution in cm EDIT to tweak movement

// Create AccelStepperWithDistance instance
AccelStepperWithDistance stepper(AccelStepper::HALF4WIRE, motorPin1, motorPin3, motorPin2, motorPin4);

// Wi-Fi and MQTT clients
WiFiClient espClient;
PubSubClient client(espClient);

// Variable for wind speed
float windSpeed = 0.0;

void restartDevice() {
  wdt_enable(WDTO_15MS); // Enable watchdog timer with a 15ms timeout
  while (true) {
    // Wait for the watchdog to reset the board
  }
}

void setup_wifi() {
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    if (millis() - startTime > 30000) {
      Serial.println("\nWi-Fi connection failed. Restarting...");
      restartDevice();
    }
  }
  Serial.println("\nWi-Fi connected. IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  if (String(topic) == wind_speed_topic) {
    windSpeed = message.toFloat();
    Serial.print("Wind Speed: ");
    Serial.println(windSpeed);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ArduinoClient")) {
      Serial.println("connected");
      client.subscribe(wind_speed_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void homeUsingLimitSwitch() {
  pinMode(limitSwitchPin, INPUT_PULLUP); // Configure limit switch pin with internal pull-up resistor

  Serial.println("Homing stepper using limit switch...");

  // Move in reverse to find the limit switch
  stepper.setMaxSpeed(1000); // Slower speed for precise homing
  stepper.setAcceleration(200);

  float homingDistance = reverseDirection ? travelDistanceCm : -travelDistanceCm; // Adjust direction based on reverseDirection
  stepper.moveToDistance(homingDistance);

  while (digitalRead(limitSwitchPin) == HIGH) { // Wait until limit switch is pressed
    stepper.run();
  }

  // Stop the motor and set the current position as zero
  stepper.stop();
  stepper.setCurrentPosition(0);

  Serial.println("Limit switch activated. Stepper position set to 0 cm.");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Initializing system...");

  // Configure stepper motor
  stepper.setMaxSpeed(2000);    // Max speed in steps per second
  stepper.setAcceleration(500); // Acceleration in steps per second^2
  stepper.setDistancePerRotation(travelPerRevolution); // Set distance per revolution

  // Home stepper using the limit switch
  homeUsingLimitSwitch();

  // Initialize Wi-Fi
  setup_wifi();

  // Initialize MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  Serial.println("System initialized.");
}

void loop() {
  // Check Wi-Fi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi disconnected. Reconnecting...");
    setup_wifi();
  }

  // Maintain MQTT connection
  if (!client.connected()) {
    reconnect();
  }

  client.loop();

  // Map wind speed (0 to 40) to stepper range (0 to travelDistanceCm)
  float clampedSpeed = constrain(windSpeed, 0.0, 55.0); // Ensure wind speed is within bounds
  float targetDistance = map(clampedSpeed, 0, 55, 0, travelDistanceCm);

  // Adjust direction based on reverseDirection
  targetDistance = reverseDirection ? -targetDistance : targetDistance;

  // Move the stepper to the target position
  stepper.moveToDistance(targetDistance);

  // Run the stepper
  while (stepper.distanceToGo() != 0) {
    stepper.run();
  }

  Serial.print("Stepper moved to distance: ");
  Serial.println(stepper.getCurrentPositionDistance());
}