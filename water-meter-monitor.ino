/**
 * Copyright 2020 Jack Higgins : https://github.com/skhg
 * All components of this project are licensed under the MIT License.
 * See the LICENSE file for details.
 */

/**
 * Water meter monitor application.
 */

/**
 * Program operates in 3 modes:
 * MONITORING - record data from sensors and send it over REST to a logging server
 * CALIBRATION - prints values to the serial console, to help setting the threshold values
 * PLOTTER - prints values to the serial console, to visualise the reflectance values and aid in debugging
 */
#define ACTIVE_RUN_MODE MONITORING

/**
 * Cold water sensor - reflectance threshold values. Estimated from CALIBRATION mode
 */
#define COLD_ACTIVE true
#define COLD_THRESHOLD_HIGH 982
#define COLD_THRESHOLD_LOW 852

/**
 * Hot water sensor - reflectance threshold values. Estimated from CALIBRATION mode
 */
#define HOT_ACTIVE true
#define HOT_THRESHOLD_HIGH 56
#define HOT_THRESHOLD_LOW 44

/**
 * When no water is flowing, how often do we report the "weather conditions" in the bathroom
 */
#define REPORT_INTERVAL_SECONDS 10

/**
 * Quantity of water represented by one-half rotation of the meter's wheel
 */
#define FLOW_DETECTION_LITRES 0.5

/**
 * IP Address and port to send data to
 */
#define SERVER_ADDRESS "http://192.168.178.29:8080"
#define URL_PATH "/data/live"

/**
 * Network hostname for the system
 */

const String HOSTNAME = "bathroom";










/**
 * System configuration values.
 * 
 * These are unlikely to be needed to be changed after development is complete
 */

/**
 * Short wait on startup to try to prevent the BMP180 sensor for returning bad data
 */
#define STARTUP_DELAY_MS 5000

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

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <home_wifi.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

/**
 * String default values
 */
#define EMPTY_STRING ""
#define HTTP_CONTENT_TYPE_HEADER "Content-Type"
#define HTTP_CONTENT_LENGTH_HEADER "Content-Length"
#define HTTP_JSON_CONTENT_TYPE "application/json"

/**
 * Configuration for saving sensor states to NVRAM
 */

#define COLD_STATE_LOC 0
#define HOT_STATE_LOC 1

/**
 * Application starts here
 */
DHT _dht(PIN_DHT_SENSOR, DHT_SENSOR_TYPE);
Adafruit_BMP085 _bmp;
WiFiClient WIFI_CLIENT;
HTTPClient HTTP_CLIENT;

enum RUN_MODES {
  CALIBRATION,
  PLOTTER,
  MONITORING
};

enum METER_STATE {
  REFLECT_HIGH,
  REFLECT_LOW
};

METER_STATE _coldState;
METER_STATE _hotState;

int _calibrationColdMin = 1024;
int _calibrationHotMin = 1024;
int _calibrationColdMax = 0;
int _calibrationHotMax = 0;

uint64_t _lastReportMillis = 0;

double _hotLitres = 0.0;
double _coldLitres = 0.0;

void setup() {
  Serial.begin(115200);

  Serial.println(EMPTY_STRING);
  Serial.println("Booting...");

  delay(STARTUP_DELAY_MS);

  if (ACTIVE_RUN_MODE == MONITORING) {
    connectToWifi();
    readSavedState(&_coldState, &_hotState);

    Serial.println("Starting sensors...");

    _dht.begin();
    _bmp.begin();
  }

  Serial.println("Configuring pins...");

  pinMode(PIN_ANALOG_IN, INPUT);

  pinMode(PIN_MULTIPLEXER_S0, OUTPUT);
  pinMode(PIN_MULTIPLEXER_S1, OUTPUT);
  pinMode(PIN_MULTIPLEXER_S2, OUTPUT);
  pinMode(PIN_MULTIPLEXER_S3, OUTPUT);

  digitalWrite(PIN_MULTIPLEXER_S0, LOW);
  digitalWrite(PIN_MULTIPLEXER_S1, LOW);
  digitalWrite(PIN_MULTIPLEXER_S2, LOW);
  digitalWrite(PIN_MULTIPLEXER_S3, LOW);

  if (ACTIVE_RUN_MODE == PLOTTER) {
    Serial.println("Cold, Hot");
  }
}

void loop() {
  bool coldFlow = COLD_ACTIVE && coldWaterFlow();
  bool hotFlow = HOT_ACTIVE && hotWaterFlow();

  switch (ACTIVE_RUN_MODE) {
    case PLOTTER: plotter(); break;
    case CALIBRATION: calibrator(); break;
    case MONITORING: monitoring(hotFlow, coldFlow); break;
  }
}

/**
 * Prints a single line of CSV values to be used with the Arduino IDE plotter mode
 */
void plotter() {
  Serial.println(EMPTY_STRING);
  delay(250);
}

/**
 * Print the calibration helper details
 */
void calibrator() {
  calibrationHelper();
  Serial.println("---");
  delay(250);
}

/**
 * Run the continuous monitoring mode. This reads from the two water meters,
 * and stores their state in case of power failure. It then reads from the other onboard
 * sensors.
 * 
 * If we have reached the interval where we must submit readings, or if water flow has
 * been detected, we submit data to the server. Otherwise we skip out.
 */
void monitoring(bool hotFlow, bool coldFlow) {
  if (hotFlow) {
    Serial.print("Hot meter is now ");
    Serial.println(meterStateToString(_hotState));

    EEPROM.write(HOT_STATE_LOC, static_cast<int>(_hotState));

    _hotLitres = _hotLitres + FLOW_DETECTION_LITRES;
  }

  if (coldFlow) {
    Serial.print("Cold meter is now ");
    Serial.println(meterStateToString(_coldState));

    EEPROM.write(COLD_STATE_LOC, static_cast<int>(_coldState));

    _coldLitres = _coldLitres + FLOW_DETECTION_LITRES;
  }

  EEPROM.commit();

  float humidity_percentage = _dht.readHumidity();
  float dht_temperature_c = _dht.readTemperature();
  float bmp_temperature_c = _bmp.readTemperature();
  float pressure_pa = _bmp.readPressure();

  String json = sensorValuesToJsonString(_hotLitres, _coldLitres,
    humidity_percentage, pressure_pa, bmp_temperature_c, dht_temperature_c);

  if (hotFlow || coldFlow || timeForReport(_lastReportMillis)) {
    _lastReportMillis = millis();

    if (sendReadings(json)) {
      // Reset the accumulated litres if we sent them
      _coldLitres = 0.0;
      _hotLitres = 0.0;
    }
  }
}

void readSavedState(METER_STATE*coldState, METER_STATE*hotState) {
  EEPROM.begin(512);

  Serial.println("Loading saved state from NVRAM...");

  int coldStored = EEPROM.read(COLD_STATE_LOC);
  int hotStored = EEPROM.read(HOT_STATE_LOC);

  /** 
   * On first boot with a new board, EEPROM values will be randomised 
   * This resets it to a known safe value
   */
  if (coldStored > 1) {
    coldStored = 0;
    EEPROM.write(COLD_STATE_LOC, 0);
  }

  /**
   * Same as above
   */
  if (hotStored > 1) {
    hotStored = 0;
    EEPROM.write(HOT_STATE_LOC, 0);
  }

  EEPROM.commit();

  *coldState = METER_STATE(coldStored);
  *hotState = METER_STATE(hotStored);

  Serial.println("Saved state:");
  Serial.println("Cold: "+ meterStateToString(*coldState));
  Serial.println("Hot: "+ meterStateToString(*hotState));
}

bool timeForReport(uint64_t lastReportMillis) {
  return (millis() - lastReportMillis) > (REPORT_INTERVAL_SECONDS * 1000);
}

/**
 * A toString helper method for logging the meter's state
 */
String meterStateToString(METER_STATE state) {
  switch (state) {
    case REFLECT_HIGH: return "SHINY";
    case REFLECT_LOW: return "DARK";
    default: return "Unknown";
  }
}

/**
 * Transmit the readings to the logging server
 */
boolean sendReadings(String json) {
  HTTP_CLIENT.begin(WIFI_CLIENT, String(SERVER_ADDRESS) + String(URL_PATH));

  HTTP_CLIENT.addHeader(HTTP_CONTENT_TYPE_HEADER, HTTP_JSON_CONTENT_TYPE);
  HTTP_CLIENT.addHeader(HTTP_CONTENT_LENGTH_HEADER, String(json.length()));
  int result = HTTP_CLIENT.POST(json);

  if (200 <= result && result < 300) {
    Serial.print("Succeeded with response code: ");
    Serial.println(result);
    return true;
  } else {
    Serial.print("Failed with response code: ");
    Serial.println(result);
    return false;
  }
}

/**
 * Convert the full set of readings to a JSON blob
 */
String sensorValuesToJsonString(double hotLitres, double coldLitres,
  float humidityPercentage, float pressurePa, float internalTemp,
  float externalTemp) {
  String content;

  const size_t capacity = 2*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(4);
  DynamicJsonDocument doc(capacity);

  JsonObject waterFlow = doc.createNestedObject("waterFlow");
  waterFlow["hotLitres"] = hotLitres;
  waterFlow["coldLitres"] = coldLitres;

  JsonObject environment = doc.createNestedObject("environment");
  environment["humidityPercentage"] = humidityPercentage;
  environment["airPressurePa"] = pressurePa;
  environment["temperatureInternalC"] = internalTemp;
  environment["temperatureExternalC"] = externalTemp;

  serializeJson(doc, content);

  return content;
}

/**
 * When running in CALIBRATION mode, this method estimates the thresholds we should
 * use, based on a floor of 20% inside the maximum/minimum recorded values
 */
void calibrationHelper() {
  Serial.println("Cold: Range " + String(_calibrationColdMin) + " to " +
  String(_calibrationColdMax));
  Serial.println("Hot: Range " + String(_calibrationHotMin) + " to " +
  String(_calibrationHotMax));

  int lowTrigger;
  int highTrigger;
  Serial.println("Recommended trigger levels (20% above and below max/min)");

  calculateThresholds(_calibrationColdMin, _calibrationColdMax, &lowTrigger,
    &highTrigger);
  Serial.println("Cold: Low: " + String(lowTrigger) + " to High " +
    String(highTrigger));

  calculateThresholds(_calibrationHotMin, _calibrationHotMax, &lowTrigger,
    &highTrigger);
  Serial.println("Hot: Low: " + String(lowTrigger) + " to High " +
    String(highTrigger));
}

void calculateThresholds(int low, int high, int*lowTrigger, int*highTrigger) {
  int range = high - low;

  int rangeBuffer = range * 0.2;
  *lowTrigger = low + rangeBuffer;
  *highTrigger = high - rangeBuffer;
}

void connectToWifi() {
  Serial.println("Connecting to WiFi...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.hostname(HOSTNAME);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println(EMPTY_STRING);
  Serial.print("Connected to ");
  Serial.print(WIFI_SSID);
  Serial.print(" with IP address ");
  Serial.println(WiFi.localIP());
}

bool coldWaterFlow() {
  digitalWrite(PIN_MULTIPLEXER_S0, LOW);
  digitalWrite(PIN_MULTIPLEXER_S1, LOW);

  return meterMoved(&_coldState, COLD_THRESHOLD_HIGH, COLD_THRESHOLD_LOW,
    &_calibrationColdMin, &_calibrationColdMax);
}

bool hotWaterFlow() {
  digitalWrite(PIN_MULTIPLEXER_S0, LOW);
  digitalWrite(PIN_MULTIPLEXER_S1, HIGH);

  return meterMoved(&_hotState, HOT_THRESHOLD_HIGH, HOT_THRESHOLD_LOW,
    &_calibrationHotMin, &_calibrationHotMax);
}

/**
 * Detect if one of the meters moved, which state it's now in, and record logging data
 * for calibration purposes
 */
bool meterMoved(METER_STATE*meterState, int highThreshold, int lowThreshold,
  int*calibrationMin, int*calibrationMax) {
  int readingValue;
  METER_STATE previousState = *meterState;
  *meterState = readMeterState(previousState, highThreshold, lowThreshold,
    &readingValue);

  if (ACTIVE_RUN_MODE == CALIBRATION) {
    *calibrationMin = min(*calibrationMin, readingValue);
    *calibrationMax = max(*calibrationMax, readingValue);
  }

  if (ACTIVE_RUN_MODE == PLOTTER) {
    Serial.print(readingValue);
    Serial.print(",");
  }

  return *meterState != previousState;
}

/**
 * Detect if the meter is showing the SHINY or DARK side
 */
METER_STATE readMeterState(METER_STATE currentState, int highThreshold,
  int lowThreshold, int*reading) {
  *reading = analogRead(PIN_ANALOG_IN);

  if (currentState == REFLECT_HIGH && *reading < lowThreshold) {
    return REFLECT_LOW;
  } else if (currentState == REFLECT_LOW && *reading >  highThreshold) {
    return REFLECT_HIGH;
  }

  return currentState;
}
