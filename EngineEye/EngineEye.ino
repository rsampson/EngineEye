// Code repository at: https://github.com/rsampson/EngineEye
//extern "C" {
//#include "user_interface.h"
//}
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <FS.h>
#include <Ticker.h>
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

// tachometer isr
inline void ICACHE_RAM_ATTR pulseISR()
{
  period = micros() - prev;
  //optional filter
//  if (period < 5000 )  //runt interrupt or > 6000 rpm, ignore
//  {
//    return;
//  }
  prev = micros();
}

// display values
float tempF_2 = 0;
float voltage = 0;
float rpm = 0;         

void handleRoot()
{
  File file = SPIFFS.open("/index.html","r");
  if(webServer.streamFile(file, "text/html")!= file.size()) {
      Serial.println("Sent less data than expected!");
  }
  file.close();
}

void handleStyle()
{
  File file = SPIFFS.open("/style.css","r");
  if( webServer.streamFile(file, "text/css")!= file.size()) {
      Serial.println("Sent less data than expected!");
  }
  file.close();
}

void handleChartStyle() 
{
  File file = SPIFFS.open("/Chart.min.css","r");
  if( webServer.streamFile(file, "text/css")!= file.size()) {
      Serial.println("Sent less data than expected!");
  }
  file.close();
}

void handleScript() 
{

  File f = SPIFFS.open("/Chart.min.js", "r");

   if (webServer.streamFile(f, "application/javascript") != f.size()) {
      Serial.println("Sent less data than expected!");
    }
    f.close();
 }


void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) { // When a WebSocket message is received
  switch (type) {
    case WStype_DISCONNECTED:             // if the websocket is disconnected
      Serial.printf("Disconnected!\n");
      break;
    case WStype_CONNECTED: {      // if a new websocket connection is established
        timer.attach(2, getData); // start sending data on the web socket
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

void getData() {
 String json = "{\"rpm\":";
     json += rpm;
     json += ",\"voltage\":";
     json += voltage;
     json += ",\"temp2\":";
     json += tempF_2;     
     json += "}";
     webSocket.broadcastTXT(json.c_str(), json.length());
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
   
    if(!SPIFFS.begin()){
      Serial.println("Error mounting SPIFFS");
      return;
    }  

    // config tachometer
    pinMode(tachoPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(tachoPin), pulseISR, RISING);
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
    webServer.on("/Chart.min.js", handleScript);
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
    
  // voltage =  analogRead(A0) * .01984;  // if analog input pin is available, can read bat voltage
/*
   sensors.requestTemperatures(); 
   tempF_2 = sensors.getTempF(insideThermometer);
   //tempF_2 = sensors.getTempFByIndex(0); // note: can't use both ds18b20 and themo 2 at same time
*/
   if(period!=0) {
      rpm = (USEC_PER_MINUT/period)/ POWER_STROKES_PER_REVOLUTION;  // account for multiple pistons firing / revolution 
    } else rpm = 0;
    period =0; // let ISR refresh this again
    // allow time for refresh
    delay(50); // this delay is required to make the web sockets work correctly
    Serial.print("period = ");
    Serial.println(period);
  }
