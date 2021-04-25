#include <ESP8266WiFi.h>
//#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <ESP8266HTTPClient.h>
#include <Adafruit_BME280.h>
#include <Wire.h>
#include <Adafruit_ADS1015.h>
#include <EEPROM.h>
#include "Adafruit_ThinkInk.h"
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSans24pt7b.h>

#include "private.h"      //Has content private to my setup, like WiFi SSID and password as well as server addresses and api keys. 

//Defined in private.h

//#define STASSID ""
//#define STAPSK  ""

//#define SERVER_IP ""

#define DEV_MODE true                 //This is used for printing various debug info to the serial port.

#define VOLTAGE_DIVIDER_FACTOR .5       //I'm using two 10k resistors in the voltage divider

#define VOLTAGE_MULTIPLIER 0.125        //Each single value on the ADS1115 ADC represents 0.125 V as configured (GAIN_ONE)

//EEPROM Address locations
#define LOOP_TRACKER        0x000     //This is the 8bit int value that tracks which loop we're on. eInk display refreshes on every third loop
#define LAST_TEMP_WHOLE     0x001     //This is the 8bit int value for the whole number part of the last temp
#define LAST_TEMP_DECIMAL   0x002     //This is the 8bit int value representing the decimal portion of the temp

#define HAS_DISPLAY true

#ifdef HAS_DISPLAY

//EINK display constants
#define TEMP_START_Y 50
#define HUMIDITY_START_Y 120
#define BATT_START_Y 65
#define BATT_START_X 160
#define BATT_WIDTH 40
#define BATT_HEIGHT 55
#define BATT_TERMINAL_WIDTH 20
#define BATT_TERMINAL_HEIGHT 10

#define EPD_BUSY    -1
#define EPD_RESET   1

#define SD_CS 2
#define SRAM_CS 3
#define EPD_CS 0
#define EPD_DC 15

ThinkInk_154_Tricolor_Z90 display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);

#endif

Adafruit_BME280 bme;
Adafruit_ADS1115 ads;

float voltage = 0;
float temperature = 0;
float humidity = 0;

//Unique to each device
String SENSOR_ID = "605aa2f0ea6fb36471aaf5a6";

void setup() {
  
  Serial.begin(115200);
  while (!Serial);   // time to get serial running
  Serial.println(F("Connecting to BME280 sensor"));
  
  EEPROM.begin(512);

  //Connect to the BME280 Sensor
  unsigned status;

  status = bme.begin(0x76);
  
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
    Serial.print("SensorID was: 0x"); Serial.println(bme.sensorID(), 16);
    Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
    Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
    Serial.print("        ID of 0x60 represents a BME 280.\n");
    Serial.print("        ID of 0x61 represents a BME 680.\n");
//    while (1) delay(10);
  } else {
    Serial.println("Successfully connected to sensor");
  }

//  Connect to the ADS1115
  ads.setGain(GAIN_ONE);
  ads.begin();

  
  Serial.println("Connecting to WiFi");

  volatile int WiFiConnectionTimeout = 0;
  Serial.println(STASSID);
  WiFi.begin(STASSID, STAPSK);

  while (WiFi.status() != WL_CONNECTED) {

    if (WiFiConnectionTimeout == 10000) {

      //log an error connecting to Wifi
      //Reset ESP8266
      Serial.println("WiFi Connection timeout exceeded. Restart ESP.");
      ESP.restart();
      
    }
          
    delay(500);
    Serial.print(".");
    WiFiConnectionTimeout += 500;
    
  }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    Serial.println("\nSuccesfully connected to Wifi!");

    delay(100);

    #ifdef DEV_MODE
    printMAC();
    #endif

    #ifdef HAS_DISPLAY
    display.begin(THINKINK_TRICOLOR);
    #endif

//    delay(100000);
  
}

void printMAC() {

  byte MAC[6];
  
  WiFi.macAddress(MAC);
  
  Serial.print("MAC Address: ");
  Serial.print(MAC[0], HEX);
  Serial.print(":");
  Serial.print(MAC[1], HEX);
  Serial.print(":");
  Serial.print(MAC[2], HEX);
  Serial.print(":");
  Serial.print(MAC[3], HEX);
  Serial.print(":");
  Serial.print(MAC[4], HEX);
  Serial.print(":");
  Serial.println(MAC[5], HEX);
  
}

void loop() {

  voltage = ads.readADC_SingleEnded(0) * VOLTAGE_MULTIPLIER / VOLTAGE_DIVIDER_FACTOR / 1000.0;
  temperature = bme.readTemperature();
  humidity = bme.readHumidity();
  
  makePostToServer();

  #ifdef HAS_DISPLAY
  //Check to see which loop this is
  //If this is the 3rd loop update the eInk display
  int loopCount = EEPROM.read(LOOP_TRACKER);

  if (loopCount % 3 == 0) {

      updateDisplay();
      
  }

  EEPROM.write(LOOP_TRACKER, loopCount + 1);
  EEPROM.commit();
  
  #endif

  
  
  Serial.println("Sleeping....");
  ESP.deepSleep(60e6, WAKE_RF_DEFAULT);
  delay(60000);
}

void updateDisplay() {

  display.clearBuffer();

  display.setCursor(0, 0);
  display.setFont();
  display.setTextSize(2);
  display.setTextColor(EPD_BLACK);
  display.print("Temperature");

  display.setFont(&FreeSansBold24pt7b);
  display.setTextSize(1);
  display.setTextColor(EPD_RED);
  display.setCursor(0, TEMP_START_Y);
  display.print(String(((temperature * 9.0 / 5.0) + 32.0), 1));

  display.fillCircle(100, TEMP_START_Y - 25, 5, EPD_RED);  //Degrees Symbol
  display.fillCircle(100, TEMP_START_Y - 25, 2, EPD_WHITE);  //Degrees Symbol

  display.setCursor(110, TEMP_START_Y);
  display.print("F");

  display.setFont();
  display.setTextSize(2);
  display.setTextColor(EPD_BLACK);
  display.setCursor(0, TEMP_START_Y + 15);
  display.print("Humidity");
  
  display.setFont(&FreeSans24pt7b);
  display.setTextSize(1);
  display.setCursor(0, HUMIDITY_START_Y);
  display.setTextColor(EPD_BLACK);
  display.print(String(humidity) + "%");

  //Battery
  int batteryMaxHeight = 45;
  int batteryHeight = int(float(batteryMaxHeight) * ((voltage-2.5)/(4.2-2.5)));
   
  
  display.fillRoundRect(BATT_START_X + ((BATT_WIDTH - BATT_TERMINAL_WIDTH)/2), BATT_START_Y - 5, 20, 10, 4, EPD_BLACK); //positive terminal
  
  display.fillRoundRect(BATT_START_X, BATT_START_Y, BATT_WIDTH, BATT_HEIGHT, 6, EPD_BLACK);
  display.fillRoundRect(BATT_START_X + 2, BATT_START_Y + 2, BATT_WIDTH - 4, BATT_HEIGHT - 5, 4, EPD_WHITE);
  
  //Battery fill
  display.fillRect(BATT_START_X+5, BATT_START_Y + 5 + batteryMaxHeight - batteryHeight, BATT_WIDTH-10, batteryHeight, EPD_RED);

  display.display();
  
}

void makePostToServer() {

  if ((WiFi.status() == WL_CONNECTED)) {

      WiFiClient client;
    HTTPClient http;

    http.begin(SERVER_IP);

    http.addHeader("Content-Type", "application/json");

    
    
    
 

    String httpData = "{\"sensorId\":\"" + SENSOR_ID + "\", \"temperature\": " + String(temperature) + ",\"humidity\": " + String(humidity) + ", \"batteryLevel\": " + String(voltage) + "}";

    int httpResponseCode = http.POST(httpData);

    if (httpResponseCode > 0) {

      Serial.print("HTTP Code: ");
      Serial.println(httpResponseCode);

      String payload = http.getString();
      Serial.println(payload); 
     
    } else {
      
      Serial.println("There was an error");
      Serial.println(http.errorToString(httpResponseCode).c_str());
      
    }

    http.end();
  }
  
}
