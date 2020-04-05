/* How to use the DHT-22 sensor with Arduino uno
   Temperature and humidity sensor
   More info: http://www.ardumotive.com/how-to-use-dht-22-sensor-en.html
   Dev: Michalis Vasilakis // Date: 1/7/2015 // www.ardumotive.com */

//Libraries
#include <DHT.h>;
#include <Wire.h>
#include <Adafruit_BMP085.h>

//Constants
#define DHTPIN 12     // what pin we're connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302)
DHT dht(DHTPIN, DHTTYPE); //// Initialize DHT sensor for normal 16mhz Arduino


//Variables
int chk;
float hum;  //Stores humidity value
float temp; //Stores temperature value
Adafruit_BMP085 bmp;


void setup()
{
  Serial.begin(9600);
  dht.begin();

  if (!bmp.begin()) {
  Serial.println("Could not find a valid BMP085 sensor, check wiring!");
  while (1) {}
  }
  
}

void loop()
{
    delay(5000);
    Serial.println("DHT22");
    //Read data and store it to variables hum and temp
    hum = dht.readHumidity();
    temp= dht.readTemperature();
    //Print temp and humidity values to serial monitor
    Serial.print("Humidity: ");
    Serial.print(hum);
    Serial.print(" %, Temp: ");
    Serial.print(temp);
    Serial.println(" Celsius");

    Serial.println("---");
    Serial.println("BMP180");


    Serial.print("Temperature = ");
    Serial.print(bmp.readTemperature());
    Serial.println(" *C");
    Serial.print("Pressure = ");
    Serial.print(bmp.readPressure());
    Serial.println(" Pa");
}

   
