#include <DHT.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <WiFi.h>
#include <PubSubClient.h>

// WiFi Settings
const char* ssid = "REMOVED";
const char* password = "REMOVED";

// MQTT broker settings
const char* mqtt_server = "broker.emqx.io";  // Use EMQX broker
const char* publish_topic = "weather_data"; // Topic to publish to
const char* subscribe_topic = "mqttx_data"; // Topic to subscribe to

WiFiClient espClient;
PubSubClient client(espClient);

// DHT11 Sensor Setup
#define DHTPIN 27       // DHT11 data pin connected to GPIO 27
#define DHTTYPE DHT11   
DHT dht(DHTPIN, DHTTYPE);

// MQ135 Sensor Setup
#define MQ135_AO_PIN 25 // Analog pin connected to AO of MQ135
#define MQ135_DO_PIN 26 // Digital pin connected to DO of MQ135

// BMP280 Sensor Setup
#define BMP280_I2C_ADDRESS 0x76 // BMP280 I2C address (default is 0x76)
#define SDA_PIN 32 // I2C SDA pin
#define SCL_PIN 33 // I2C SCL pin
Adafruit_BMP280 bmp;

// LED Pin Setup
#define LED_PIN 13 // LED connected to GPIO 13

// Variables to store readings
float temperature = 0.0;
float humidity = 0.0;
float pressure = 0.0;
float altitude = 0.0;
int gasValue = 0;
bool gasDetected = false;

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  // Initialize DHT11
  dht.begin();
  Serial.println("DHT11 sensor initialized");

  // Initialize BMP280 
  Wire.begin(SDA_PIN, SCL_PIN); // Initialize I2C with custom SDA and SCL pins
  if (!bmp.begin(BMP280_I2C_ADDRESS)) {
    Serial.println("Could not find BMP280 sensor!");
  }
  Serial.println("BMP280 sensor initialized");

  // Initialize MQ135
  pinMode(MQ135_DO_PIN, INPUT); // Set DO pin as input for threshold detection
  pinMode(LED_PIN, OUTPUT);     // Set LED pin as output
  Serial.println("MQ135 gas sensor ready");

  // Connect to WiFi
  connectToWiFi();

  // Set up MQTT client
  client.setServer(mqtt_server, 1883);
  client.setCallback(mqttCallback);

  // Subscribe to mqttx_data topic
  client.subscribe(subscribe_topic);
}

void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi!");
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Callback function for handling incoming MQTT messages
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(" with message: ");
  
  // Print the incoming message
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void loop() {
  // Reconnect to MQTT if disconnected
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  // Read data from DHT11
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" °C");

  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");


  // Read data from BMP280
  pressure = bmp.readPressure() / 100.0F; // Read pressure (in hPa) from BMP280
  altitude = bmp.readAltitude(1013.25);  // Calculate altitude (sea level pressure = 1013.25 hPa)

  Serial.print("Pressure: ");
  Serial.print(pressure);
  Serial.println(" hPa");

  Serial.print("Altitude: ");
  Serial.print(altitude);
  Serial.println(" m");

  // Read data from MQ135
  gasValue = analogRead(MQ135_AO_PIN); // Read analog value from MQ135
  gasValue = 1073;
  gasDetected = digitalRead(MQ135_DO_PIN) == HIGH; // Check if gas level exceeds threshold

  Serial.print("Gas Concentration: ");
  Serial.print(gasValue);
  Serial.println(" ppm");

  // Threshold Detection
  if (gasDetected) {
    Serial.println("Gas level exceeds threshold!");
    digitalWrite(LED_PIN, HIGH); // Turn LED ON
  } else {
    Serial.println("Gas level is normal.");
    digitalWrite(LED_PIN, LOW);  // Turn LED OFF
  }

  // Prepare payload for MQTT
  String payload = createPayload();

  // Publish the data to MQTT broker
  client.publish(publish_topic, payload.c_str());

  // Delay before next reading
  delay(2000);
}

String createPayload() {
  // Create a JSON payload
  String payload = "{";
  payload += "\"temperature\": " + String(temperature) + ","; 
  payload += "\"humidity\": " + String(humidity) + ","; 
  payload += "\"pressure\": " + String(pressure) + ","; 
  payload += "\"altitude\": " + String(altitude) + ","; 
  payload += "\"gas_value\": " + String(gasValue) + ","; 
  payload += "}"; 
  return payload; 
}

void reconnectMQTT() {
  // Loop until we're reconnected to the MQTT broker
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client")) {
      Serial.println("connected");
      client.subscribe(subscribe_topic);  // Re-subscribe to the mqttx_data topic if disconnected
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}
