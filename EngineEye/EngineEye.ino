

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
#include <Ticker.h>
#include <SimplyAtomic.h>
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

byte tachoPin  = D5;       // on esp8266, GPIO 14, tachometer input
unsigned long period;      // period between pulses
unsigned long prev;        // previous time
unsigned long discharge;   // time coil is discharged

// tachometer isr
inline void ICACHE_RAM_ATTR pulseISR()
{
  if(digitalRead(tachoPin) == HIGH) {
    period = micros() - prev;
    //optional filter
    //  if (period < 5000 )  //runt interrupt or > 6000 rpm, ignore
    //  {
    //    return;
    //  }
    prev = micros();
  } else {  // tach signal is low
    discharge = micros() - prev;
  }
}

// display values
float tempF_2 = 0;
float voltage = 0;
float rpm = 0;
float dwell = 0;

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

void handleStyle()
{
    handleFile("/style.css","text/css"); 
}

void handleChartStyle()
{
    handleFile("/Chart.min.css","text/css"); 
}

void handleScript()   // might be able to send as .gz compressed file
{
  handleFile("/Chart.min.js.gz","application/javascript");  
}

void getData() {  // form a json description of the data and broadcast it on a web socket
  String json = "{\"rpm\":";
  json += rpm;
  json += ",\"voltage\":";
  json += voltage;
  json += ",\"temp2\":";
  json += tempF_2;
  json += "}";
  webSocket.broadcastTXT(json.c_str(), json.length());
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) { // When a WebSocket message is received
  switch (type) {
    case WStype_DISCONNECTED:             // if the websocket is disconnected
      Serial.printf("Disconnected!\n");
      break;
    case WStype_CONNECTED: {      // if a new websocket connection is established
        timer.attach(1, getData); // start sending data on the web socket
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
  //WiFi.mode(WIFI_AP);
  //WiFi.setOutputPower(20); // Ras hack
  WiFi.softAP("engine", "", 4); // set to ch 4, no password

  //    WiFi.mode(WIFI_STA);
  //    WiFi.begin("offline" ,"2LiveCrew");
  //    while(WiFi.status()!=WL_CONNECTED)
  //    {
  //      Serial.print(".");
  //      delay(500);
  //    }

  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request, IP address will be default (192.168.4.1)
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  if (!SPIFFS.begin()) {
    Serial.println("Error mounting SPIFFS");
    return;
  }

  // config tachometer
  pinMode(tachoPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(tachoPin), pulseISR, CHANGE);
  period = 0;
  prev = 0;
  /*
      // Start up the library for DS18B20
      sensors.begin();
      if (!sensors.getAddress(insideThermometer, 0)) Serial.println("Unable to find address for Device 0");
      // set the resolution to 12 bit (Each Dallas/Maxim device is capable of several different resolutions)
      sensors.setResolution(insideThermometer, 12);
  */
  webServer.on("/Chart.min.css", handleChartStyle);
  webServer.on("/Chart.min.js.gz", handleScript);
  webServer.on("/style.css", handleStyle);
  webServer.onNotFound(handleRoot);
  webServer.begin();

  webSocket.onEvent(webSocketEvent);
  webSocket.begin();

  Serial.println("Starting servers");
  delay(100);
}

#define USEC_PER_MINUT 60000000

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
     tempF_2 = sensors.getTempF(insideThermometer);
     //tempF_2 = sensors.getTempFByIndex(0); // note: can't use both ds18b20 and themo 2 at same time
  */
  if (period != 0) {
    ATOMIC() {  // probably not needed
      rpm = (USEC_PER_MINUT / period) / POWER_STROKES_PER_REVOLUTION; 
      dwell = (period - discharge)  * 360;
      dwell = dwell / (period * POWER_STROKES_PER_REVOLUTION);
    }
  } else {
    rpm = 0;
    dwell=0;
  }
  period = 0; // let ISR refresh this again
  // allow time for refresh
  delay(100); // this delay is required to make the web sockets work correctly
  Serial.print(" dwell = ");
  Serial.println(dwell);
}
