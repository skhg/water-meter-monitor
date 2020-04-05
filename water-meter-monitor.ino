/**
 * Water meter monitor application.
 * 
 * Requires home_wifi.h to be present in Arduino libraries - imports Wifi hostname and password
 */


/**
 * User-configurable values start here:
 */

/**
 * Cold water sensor - reflectance threshold values
 */
#define COLD_ACTIVE true
#define COLD_THRESHOLD_HIGH 951
#define COLD_THRESHOLD_LOW 789

/**
 * Hot water sensor - reflectance threshold values
 */
#define HOT_ACTIVE true
#define HOT_THRESHOLD_HIGH 53
#define HOT_THRESHOLD_LOW 42

/**
 * Quantity of water represented by one-half rotation of the meter's wheel
 */
#define FLOW_DETECTION_LITRES 0.5

/**
 * Enable/disable Serial output to aid in calibration of the reflectivity sensors
 */
#define CALIBRATION 1

/**
 * IP Address and port to send data to
 */
#define IP_1 192
#define IP_2 168
#define IP_3 178
#define IP_4 29
#define IP_PORT 8080

/**
 * No need to make further changes below here
 */










/**
 * System configuration values.
 * 
 * These are unlikely to be needed to be changed after development is complete
 */

/**
 * Short wait on startup to try to prevent the BMP180 sensor for returning bad data
 */
#define STARTUP_DELAY_MS 5000

/**
 * Delay between taking readings
 */
#define LOOP_DELAY_MS 500

/**
 * Delay before reboot
 */
#define REBOOT_DELAY_MS 1000

/*
  NodeMCU v3 Arduino/Physical pinout map
  
  Arduino | NodeMCU | Available for use
  0       | D3      | M_S2
  1       | TX      | X
  2       | D4      | M_S3
  3       | RX      | X
  4       | D2      | BMP180
  5       | D1      | BMP180
  6       |         | X
  7       |         | X
  8       |         | X
  9       | S2      | X
  10      | S3      | X
  11      |         | X
  12      | D6      | DHT22
  13      | D7      | M_S0
  14      | D5      | ?
  15      | D8      | M_S1
  16      | D0      | ?
 */
#define PIN_ANALOG_IN A0
#define PIN_MULTIPLEXER_S0 13
#define PIN_MULTIPLEXER_S1 15
#define PIN_MULTIPLEXER_S2 0
#define PIN_MULTIPLEXER_S3 2
#define PIN_DHT_SENSOR 12
#define DHT_SENSOR_TYPE DHT22

#include<ESP8266WiFi.h> 
#include<home_wifi.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <EEPROM.h>


/**
 * Configuration for saving sensor states to NVRAM
 */

#define COLD_STATE_LOC 0
#define HOT_STATE_LOC 1

/**
 * Application starts here
 */
DHT dht(PIN_DHT_SENSOR, DHT_SENSOR_TYPE);
Adafruit_BMP085 bmp;
WiFiClient client;

enum METER_STATE {
  REFLECT_HIGH,
  REFLECT_LOW
};

IPAddress server(IP_1,IP_2,IP_3,IP_4);

METER_STATE coldState_nvram;
METER_STATE coldState;
METER_STATE hotState_nvram;
METER_STATE hotState;

void setup() {
  Serial.begin(9600);
  EEPROM.begin(512);

  int coldStored = EEPROM.read(COLD_STATE_LOC);
  int hotStored = EEPROM.read(HOT_STATE_LOC);

  if(coldStored > 1){
    coldStored = 0;
    EEPROM.write(COLD_STATE_LOC, 0);
    EEPROM.commit();
  }

  if(hotStored > 1){
    hotStored = 0;
    EEPROM.write(HOT_STATE_LOC, 0);
    EEPROM.commit();
  }

  coldState = METER_STATE(coldStored);
  coldState_nvram = METER_STATE(coldStored);
  hotState = METER_STATE(hotStored);
  hotState_nvram = METER_STATE(hotStored);

  Serial.println("Cold state: "+ String(coldStored));
  Serial.println("Hot state: "+ String(hotStored));
  
  delay(STARTUP_DELAY_MS);
  dht.begin();
  bmp.begin();
  
  pinMode(PIN_ANALOG_IN, INPUT);

  pinMode(PIN_MULTIPLEXER_S0, OUTPUT);
  pinMode(PIN_MULTIPLEXER_S1, OUTPUT);
  pinMode(PIN_MULTIPLEXER_S2, OUTPUT);
  pinMode(PIN_MULTIPLEXER_S3, OUTPUT);

  digitalWrite(PIN_MULTIPLEXER_S0, LOW);
  digitalWrite(PIN_MULTIPLEXER_S1, LOW);
  digitalWrite(PIN_MULTIPLEXER_S2, LOW);
  digitalWrite(PIN_MULTIPLEXER_S3, LOW);
}

void loop() {
  connectToWifi();

  bool coldFlow = COLD_ACTIVE && coldMoved();
  bool hotFlow = HOT_ACTIVE && hotMoved();

  if(CALIBRATION) {
    Serial.println();
  }
  
  double hotLitres = 0.0;
  double coldLitres = 0.0;

  if(hotFlow){
    hotLitres = FLOW_DETECTION_LITRES;
  }

  if(coldFlow){
    coldLitres = FLOW_DETECTION_LITRES;
  }
  
  float humidity_percentage = dht.readHumidity();
  float dht_temperature_c = dht.readTemperature();
  float bmp_temperature_c = bmp.readTemperature();
  float pressure_pa = bmp.readPressure();

  String json = "{\"waterFlow\":{\"hotLitres\":"+ String(hotLitres)+",\"coldLitres\":"+String(coldLitres)+"},\"environment\":{\"humidityPercentage\":"+humidity_percentage+",\"airPressurePa\":"+pressure_pa+",\"temperatureInternalC\":"+bmp_temperature_c+",\"temperatureExternalC\":"+dht_temperature_c+"}}";

  if (client.connect(server, IP_PORT)) {      
      // Make a HTTP request:
      client.println("POST /data/live HTTP/1.0");
      client.println("Content-Type: application/json");
      client.print("Content-Length: ");
      client.println(json.length());
      client.println();
      client.println(json);
      client.println();
      client.stop();
    }else{
      Serial.println("ERROR: Failed to connect");
      reboot();
    }

    delay(LOOP_DELAY_MS);
}


void reboot(){
  if(hotState != hotState_nvram){
    EEPROM.write(HOT_STATE_LOC, int(hotState));
    Serial.println("Storing hot state: " + String(int(hotState)));
    Serial.println("Stored hot state: " + String(EEPROM.read(HOT_STATE_LOC)));
  }

  if(coldState != coldState_nvram){
    EEPROM.write(COLD_STATE_LOC, int(coldState));
    Serial.println("Storing cold state: " + String(int(coldState)));
    Serial.println("Stored cold state: " + String(EEPROM.read(COLD_STATE_LOC)));
  }
  EEPROM.commit();
  
  delay(REBOOT_DELAY_MS);
  Serial.println("Rebooting...");
  ESP.restart();
}


void connectToWifi(){
  if(WiFi.status() == WL_CONNECTED){
    return;  
  }
  
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID); 
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    reboot();
  }
  
  Serial.println("");
  Serial.println("WiFi connected"); 
}

bool coldMoved(){
  digitalWrite(PIN_MULTIPLEXER_S0, LOW);
  digitalWrite(PIN_MULTIPLEXER_S1, LOW);

  METER_STATE newState = takeReading(coldState, COLD_THRESHOLD_HIGH, COLD_THRESHOLD_LOW);
  
  if(newState != coldState){
    coldState = newState;
    return true;
  }

  return false;
}

bool hotMoved(){
  digitalWrite(PIN_MULTIPLEXER_S0, LOW);
  digitalWrite(PIN_MULTIPLEXER_S1, HIGH);

  METER_STATE newState = takeReading(hotState, HOT_THRESHOLD_HIGH, HOT_THRESHOLD_LOW);
  
  if(newState != hotState){
    hotState = newState;
    return true;
  }

  return false;
}

METER_STATE takeReading(METER_STATE currentState, int highThreshold, int lowThreshold){
  int reading = analogRead(PIN_ANALOG_IN);

  if(CALIBRATION) {
    Serial.print(reading);
    Serial.print(",");
  }
  
  if(currentState == REFLECT_HIGH && reading < lowThreshold){
    return REFLECT_LOW;
  }else if(currentState == REFLECT_LOW && reading >  highThreshold){
    return REFLECT_HIGH;
  }

  return currentState;
}
