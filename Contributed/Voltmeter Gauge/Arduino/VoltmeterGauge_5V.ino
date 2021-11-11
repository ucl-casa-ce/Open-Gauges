
// The Arduino Libraries to include

#include <SPI.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>



// Set voltage interger
int voltage;
// Set Sweep interger (sweep run on power up)
int s = 0;
// Set interger for the data feed - in our case called Wind as its wind gusts in mph
int wind;

// Set up Wifi and Connection to an MQTT Server for the data

const char* ssid = "YourWIFI";
const char* password =  "WIFIPassword";
const char* mqttServer = "YourMQTTServerandPortBelow";
const int mqttPort = 1884;
const char* mqttUser = "Yourusername";
const char* mqttPassword = "Yourpassword";

// Set Client Connection Name - this can be any name
WiFiClient NodeWindVoltage;
PubSubClient client(NodeWindVoltage);




//Main Loop

void setup()
{

// set pins for voltage output
  pinMode(3,OUTPUT);
  Serial.begin(115200);
  
// Start up Sweep - Needle moves smoothly from 0 - 5 V and back again - 255 = 5V

while (s < 255)
 {
analogWrite(3,s);
s = (s + 1);
delay(50);
}

s = 255;
while (s > 0)
 {
analogWrite(3,s);
s = (s - 1);
delay(50);
}
  
// Connect to Wifi and MQTT

  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");
  client.setServer(mqttServer, mqttPort);

  client.setCallback(callback);
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect("NodeWindVoltage", mqttUser, mqttPassword))
    {
      Serial.println("connected");
    } else {
      Serial.print("failed state ");
      Serial.print(client.state());
      delay(2000);
    }
  }

// MQTT Topic to Subscribe to - Edit to add yours, our was weather/windsSpeed_mph

// Edit for your MQTT Feed 
 client.subscribe("personal/ucfnaps/downhamweather/windSpeed_mph");
}

//Reconnect if connection lost

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...

      // ... and resubscribe
      client.subscribe("personal/ucfnaps/downhamweather/windSpeed_mph");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


// What to do when a message arrives

void callback(const char* topic, byte* payload, unsigned int length) {
  
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
   
  }
  
  Serial.println();

// Convert message payload to a string and then to an interger to negate error reading bytes
  
  payload[length] = '\0';
  String w = String((char*)payload);
  int wind = w.toInt();
  
  
// Print out output  
  Serial.print("wind = ");
  Serial.println(wind);

// Map our data range to the voltage - in our case, Wind at 0 to 40 mph to 0 to 255 (0-5v)

  voltage = map(wind, 0, 38, 0, 255);
  
// Output the voltage
  analogWrite(3,voltage);
 
  delay (1000);

}

void loop()

// Loop the MQTT Connected and Reconnect if Connection is Lost

{
 if (!client.connected()) {
    client.connect(" Nodevoltagewind", mqttUser, mqttPassword);
// Edit According to your feed
   
    client.subscribe("personal/ucfnaps/downhamweather/windSpeed_mph");
  }

  client.loop();
}
