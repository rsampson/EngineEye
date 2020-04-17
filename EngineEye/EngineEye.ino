// Code repository at: https://github.com/rsampson/EngineEye

// chart and websockets inspired by://https://github.com/acrobotic/Ai_Demos_ESP8266

//extern "C" {
//#include "user_interface.h"
//}
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <FS.h>
#include <Ticker.h>      // https://github.com/esp8266/Arduino/tree/master/libraries/Ticker
#include <interrupts.h>
#include <ArduinoSort.h> // https://github.com/emilv/ArduinoSort

// sensor libraries
#include <OneWire.h>
#include <DallasTemperature.h>

#define POWER_STROKES_PER_REVOLUTION 2  // for air cooled 4 cy VW

Ticker timer;

OneWire oneWire(D6);  // sensor hooked to D6, gpio 12
DallasTemperature sensors(&oneWire);
// arrays to hold device address
DeviceAddress insideThermometer;

// Captive portal code from: https://github.com/esp8266/Arduino/blob/master/libraries/DNSServer/examples/CaptivePortal/CaptivePortal.ino
const byte DNS_PORT = 53;
DNSServer dnsServer;
ESP8266WebServer webServer(80);
WebSocketsServer webSocket = WebSocketsServer(81);

byte tachoPin  = D5;  // on esp8266, GPIO 14, tachometer input

volatile unsigned long period;      // period between pulses
volatile unsigned long prev;        // previous time signal was low
volatile unsigned long periodArray[17];      // period between pulses


// tachometer isr
inline void ICACHE_RAM_ATTR pulseISR()
{
   digitalWrite(D6, HIGH);
   periodArray[8] = micros() - prev;

   int sample = 0;
   // make sure tach input is stable high before preceeding
  for (int i =0; i < 100; i++)
   {
      if (digitalRead(tachoPin) == HIGH) 
      {
        sample++;
      } 
   }
   if(sample < 75) return;
   prev = micros();            // filter the value by taking the middle 
   sortArray(periodArray, 17); // value of a sorted array
   period = periodArray[8];
 
   digitalWrite(D6, LOW);
}

void handleFile(const String& file, const String& contentType)
{
  File f = SPIFFS.open(file, "r");

  if (webServer.streamFile(f, contentType) != f.size()) {
    Serial.println("Sent less data than expected!");
  }
  f.close();
}

void handleRoot()
{
    handleFile("/index.html","text/html"); 
}

void handleRPM()
{
    handleFile("/rpm.html","text/html"); 
}

void handleStyle()
{
    handleFile("/style.css","text/css"); 
}

void handleRpmStyle()
{
    handleFile("/rpmstyle.css","text/css"); 
}

void handleChartStyle()
{
    handleFile("/Chart.min.css","text/css"); 
}

void handleScript()   // might be able to send as .gz compressed file
{
  handleFile("/Chart.min.js.gz","application/javascript");  
}

// display values -- initialize with reasonable values
float tempF = 70;
float voltage = 14;
float rpm = 900;
float dwell = 50;
float finalrpm = 900;
float rpm_array[16];


void getData() {  // form a json description of the data and broadcast it on a web socket
  String json = "{\"rpm\":";
  json += String(finalrpm, 0);
  json += ",\"voltage\":";
  json += String(voltage, 1);
  json += ",\"temp2\":";
  json += String(tempF,0);
  json += ",\"dwell\":";
  json += String(dwell,0);
  json += "}";
  webSocket.broadcastTXT(json.c_str(), json.length());
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) { // When a WebSocket message is received
  switch (type) {
    case WStype_DISCONNECTED:             // if the websocket is disconnected
      timer.detach();
      Serial.printf("Disconnected!\n");
      break;
    case WStype_CONNECTED: {      // if a new websocket connection is established
        timer.attach(.5, getData); // start sending data on the web socket 2 times a sec
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("Connected from %d.%d.%d.%d url: %s\n", ip[0], ip[1], ip[2], ip[3], payload);
      }
      break;
    default: {
        Serial.print("WStype = ");
        Serial.println(type);
      }
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  //ESP.eraseConfig();
  WiFi.disconnect(true);
  //wifi_set_phy_mode(PHY_MODE_11B);  //This is supposed to add more power
  //WiFi.setOutputPower(20); // Ras hack
  WiFi.softAP("engine", "", 4); // set to ch 4, no password

  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request, IP address will be default (192.168.4.1)
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  if (!SPIFFS.begin()) {
    Serial.println("Error mounting SPIFFS");
    return;
  }

  /*
      // Start up the library for DS18B20
      sensors.begin();
      if (!sensors.getAddress(insideThermometer, 0)) Serial.println("Unable to find thermo address");
      // set the resolution to 12 bit 
      sensors.setResolution(insideThermometer, 12);
  */
  webServer.on("/Chart.min.css", handleChartStyle);
  webServer.on("/Chart.min.js.gz", handleScript);
  webServer.on("/style.css", handleStyle);
  webServer.on("/rpmstyle.css", handleRpmStyle);
  webServer.on("/rpm.html", handleRPM);
  webServer.onNotFound(handleRoot);
  webServer.begin();

  webSocket.onEvent(webSocketEvent);
  webSocket.begin();

  Serial.println("Starting servers");
  delay(100);
   // config tachometer
  pinMode(D6, OUTPUT);
  pinMode(tachoPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(tachoPin), pulseISR, RISING);
  period = 0;
  prev = 0;
}

#define USEC_PER_MINUT 60000000

unsigned long highCount = 0;
unsigned long lowCount = 0;

void loop() {
  dnsServer.processNextRequest(); // captive portal support
  yield();
  webServer.handleClient();
  yield();
  webSocket.loop();
  yield();

  voltage =  analogRead(A0) * .01984;  // if analog input pin is available, can read bat voltage
  /*
     sensors.requestTemperatures();
     tempF = sensors.getTempF(insideThermometer);
     //tempF = sensors.getTempFByIndex(0); // note: can't use both ds18b20 and themo 2 at same time
  */
  // compute RPM
  { 
    InterruptLock lock; 
    if (period != 0) {
        rpm = (USEC_PER_MINUT / period) / POWER_STROKES_PER_REVOLUTION;
     } else {
      rpm = 0;
     }
   }

   if(rpm > 500 && rpm < 5000) {      // engine can only do this
     rpm_array[8] = rpm;              // filter out outliers 
     sortArray(rpm_array, 16);
     rpm = rpm_array[8];
  
     finalrpm = (rpm + (3 * finalrpm)) / 4;  // do some crude averaging
   }
   period = 0; // let ISR refresh this again

   // compute dwell by statistical sampling
  if(digitalRead(tachoPin) == HIGH) {
    highCount++;
  } else {
    lowCount++;
  }
  // decimate counts to make measurement more responsive
  if((highCount + lowCount) > 1000) {
    highCount = highCount / 2;
    lowCount = lowCount / 2;
  }
  dwell = (highCount  * 180) / (highCount + lowCount);
   
  // allow time for refresh of tach sampling, make random so not synchronous with tach signal
  delay(random(50, 100)); //  delay also required to make the web sockets work correctly
  //Serial.print(" dwell = ");
  //Serial.println(dwell);
}
