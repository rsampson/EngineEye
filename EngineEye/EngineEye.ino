// Code repository at: https://github.com/rsampson/EngineEye
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <EspHtmlTemplateProcessor.h> // https://github.com/plapointe6/EspHtmlTemplateProcessor
#include <Effortless_SPIFFS.h>        // https://github.com/thebigpotatoe/Effortless-SPIFFS

#define POWER_STROKES_PER_REVOLUTION 2  // for air cooled VW
//#define USE_THERMOCOUPLE 1
#define USE_DS18B20 1

#ifdef USE_DS18B20
#include <OneWire.h>
#include <DallasTemperature.h>
// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS D6  // gpio 12
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
#endif

#ifdef USE_THERMOCOUPLE
// if using a thermocouple, you may want to consider using the MAX31856 module 
// (library at https://github.com/adafruit/Adafruit_MAX31856) instead of the older max6675
#include <max6675.h> // www.ladyada.net/learn/sensors/thermocouple
// must set board to wemos d1 mini or similar in order to use these pin definitions
#define thermoDO D6
#define thermoCS_1 D7
#define thermoCS_2 D4
#define thermoCLK D8
MAX6675 thermocouple_1(thermoCLK, thermoCS_1, thermoDO);
MAX6675 thermocouple_2(thermoCLK, thermoCS_2, thermoDO);
#endif

// Captive portal code from: https://github.com/esp8266/Arduino/blob/master/libraries/DNSServer/examples/CaptivePortal/CaptivePortal.ino
const byte DNS_PORT = 53;
DNSServer dnsServer;
ESP8266WebServer webServer(80); 
EspHtmlTemplateProcessor templateProcessor(&webServer); 

byte tachoPin  = D5;       // D5 on esp8266, GPIO 14, tachometer input
unsigned long period;      // period between pulses
unsigned long prev;        // previous time
unsigned long lastDisplay; // counter for display time
unsigned int rpm;          // rpm to be printed

// tachometer isr
inline void ICACHE_RAM_ATTR pulseISR()
{
  period = (micros()-prev);
  prev = micros();
}

float tempF_1 = 0;
float tempF_2 = 0;
float voltage = 0;
float revs = 0;

String indexKeyProcessor(const String& key)
{
  if (key == "TEMP1F") return String(tempF_1);
  else if (key == "TEMP2F") return String(tempF_2);
  else if (key == "VOLTAGE") return String(voltage);
  else if (key == "REVS") return String(revs);
  
  return "Key not found";
}

void handleRoot()
{
  templateProcessor.processAndSend("/index.html", indexKeyProcessor);
}

eSPIFFS fileSystem(&Serial);
void handleStyle() 
{
  size_t fileSize = fileSystem.getFileSize("/style.css");
  String fileContents;
  fileSystem.openFromFile("/style.css", fileContents);
  webServer.send(200, "text/css", fileContents);
}


void setup() {
    Serial.begin(115200);
    delay(100);
    
    if(!SPIFFS.begin()){
      Serial.println("An Error has occurred while mounting SPIFFS");
      return;
    }  
    
    // config tachometer
    pinMode(tachoPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(tachoPin), pulseISR, RISING);
    period = 0;
    prev = 0;
    lastDisplay = 0;

    WiFi.mode(WIFI_AP);
    WiFi.softAP("engine");

    // if DNSServer is started with "*" for domain name, it will reply with
    // provided IP to all DNS request
    // IP address will be default (192.168.4.1)
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP()); 
     
#ifdef USE_DS18B20
    // Start up the library for DS18B20
    sensors.begin();
#endif
    // replay to all requests with same HTML or load style.css file
    webServer.on("/style.css", handleStyle);
    webServer.onNotFound(handleRoot);
    delay(100);
    Serial.println("Starting server");
    webServer.begin();
}

void loop() {
   // captive portal support
   dnsServer.processNextRequest();
   webServer.handleClient();
   delay(100); // this delay is required to make the captive portal work correctly
      
   voltage =  analogRead(A0) * .01984;  // if analog input pin is available, can read bat voltage

#ifdef USE_THERMOCOUPLE
   tempF_1 = thermocouple_1.readFahrenheit();
   tempF_1 = (41 * tempF_1) /43;  // fudge factor to use type T thermocouple if needed
   delay(250);
   tempF_2 = thermocouple_2.readFahrenheit();
   tempF_2 = (41 * tempF_2) /43;  // fudge factor to use type T thermocouple if needed
   delay(250);
#endif

#ifdef USE_DS18B20
   sensors.requestTemperatures(); // Send the command to get temperatures
   tempF_2 = sensors.getTempFByIndex(0); // note: can't use both ds18b20 and themo 2 at same time
   //tempF_2 = ds.getTempF();
#endif   

   // tachometer processing, each 250 ms
  if( (millis() - lastDisplay) >= 250 )
  {
    lastDisplay = millis();
    if(period!=0)
      rpm = (unsigned int) 60000000/period;
    else
      rpm = 0;
  }
  revs = rpm / POWER_STROKES_PER_REVOLUTION; // clean this up
    
   //WiFi.printDiag(Serial);
 }
