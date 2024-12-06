
#include <ServoEasing.hpp>
#include <WiFiClient.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Adafruit_NeoPixel.h>

//Define Device Connection Name



// Define NeoPixel strip configuration
#define LED_COUNT 47
#define LED_PIN 5
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// Define servo and configuration variables
ServoEasing servo;
const int servoMinAngle = 10;   // Minimum angle for the servo
int servoMaxAngle = 160;        // Maximum angle for the servo (adjustable)
float windSpeedMax = 60.0;      // Maximum wind speed for mapping (adjustable)

// WiFi and MQTT configuration
const char* ssid = ""; // Your Wifi
const char* password = ""; // Your Wifi
const char* mqttServer = "mqtt.cetools.org"; // Our Open MQTT Broker - Edit as needs be
const int mqttPort = 1883;

WiFiClient espWindSpeedLecture24; 
PubSubClient client(espWindSpeedLecture24);

// Function prototypes
void callback(const char* topic, byte* payload, unsigned int length);
void reconnect();

void setup() {
  Serial.begin(115200);

  strip.begin();
  uint32_t white = strip.Color(255, 255, 255);
  strip.fill(white, 0, 27);
  strip.show();

  servo.attach(4);
  servo.setEasingType(EASE_LINEAR);
  servo.setSpeed(15);

  // Perform servo calibration sweep
  Serial.println("Performing calibration sweep");
  Serial.print("Moving to min angle: ");
  Serial.println(servoMinAngle);
  servo.easeTo(servoMinAngle);
  delay(10000);

  Serial.print("Moving to max angle: ");
  Serial.println(servoMaxAngle);
  servo.easeTo(servoMaxAngle);
  delay(1000);

  Serial.print("Returning to min angle: ");
  Serial.println(servoMinAngle);
  servo.easeTo(servoMinAngle);
  delay(5000);

  WiFi.mode(WIFI_STA);
  WiFi.softAPdisconnect(true);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to the WiFi network");

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect("espWindSpeedLecture24")) {
      Serial.println("Connected to MQTT server");
      client.subscribe("personal/ucfnaps/downhamweather/windgust1min");
    } else {
      Serial.print("MQTT connection failed, state: ");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println("Connected to MQTT server");
      client.subscribe("personal/ucfnaps/downhamweather/windgust1min");
    } else {
      Serial.print("Failed to connect to MQTT, state: ");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

void callback(const char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  String message;
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    message += (char)payload[i];
  }
  Serial.println();

  if (String(topic) == "personal/ucfnaps/downhamweather/windgust1min") {
    float windSpeed = message.toFloat();
    if (windSpeed >= 0 && windSpeed <= windSpeedMax) {
      // Map wind speed (0–windSpeedMax) to servo angle (servoMinAngle–servoMaxAngle)
      int angle = map(windSpeed, 0, windSpeedMax, servoMinAngle, servoMaxAngle);
      Serial.print("Mapped wind speed: ");
      Serial.print(windSpeed);
      Serial.print(" mph to servo angle: ");
      Serial.println(angle);
      servo.easeTo(angle);
    } else {
      Serial.println("Invalid wind speed received.");
    }
  }
}