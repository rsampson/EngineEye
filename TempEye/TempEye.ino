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

#ifdef DS18B20
// sensor libraries
#include <OneWire.h>
#include <DallasTemperature.h>
OneWire oneWire(2);  // sensor hooked to gpio 2, esp-01
// OneWire oneWire(D7);  // sensor hooked to D7, gpio 13, wemos d1 mini
DallasTemperature sensors(&oneWire);
// arrays to hold device address
DeviceAddress thermo;
#endif

// Captive portal code from: https://github.com/esp8266/Arduino/blob/master/libraries/DNSServer/examples/CaptivePortal/CaptivePortal.ino
const byte DNS_PORT = 53;
DNSServer dnsServer;
ESP8266WebServer webServer(80);
WebSocketsServer webSocket = WebSocketsServer(81);
Ticker timer;      // send data on websocket each tick

void handleFile(const String& file, const String& contentType)
{
  File f = SPIFFS.open(file, "r");
  if (!f) {
    Serial.println("***Error opening " + file + " ***");
    webServer.send(200, "text/plain", "error sending " + file);
  } else {
    if (webServer.streamFile(f, contentType) != f.size()) {
      Serial.println("Sent less data than expected!");
    }
  }
  f.close();
}

void handleRoot()
{
  handleFile("/index.html", "text/html");
}

void handleStyle()
{
  handleFile("/style.css", "text/css");
}

void handleChartStyle()
{
  handleFile("/Chart.min.css", "text/css");
}

void handleScript()
{
  handleFile("/Chart.min.js.gz", "application/javascript");
}

void handleGaugeScript()
{
  handleFile("/gauge.min.js", "application/javascript");
}

// display values -- initialize with reasonable values
float tempF = 70;
int sensorValue = 570;

void getData() {  // form a json description of the data and broadcast it on a web socket
#ifdef DS18B20
  sensors.requestTemperatures();
  tempF = sensors.getTempF(thermo);
#else  // using a diode connected to the analog pin
  sensorValue = analogRead(A0);
  // map diode voltage to temperature F
  tempF = map(sensorValue, 625, 400, 32, 212); // 32 deg was .650v
#endif

  String json = "{\"temp2\":";
  json += String(tempF, 1);
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
        timer.attach(1, getData); // start sending data on the web socket once a sec
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
  Serial.println("started");

  //ESP.eraseConfig();
  WiFi.softAP("engine", "", 4); // set to ch 4, no password
  Serial.println("connected");

  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request, IP address will be default (192.168.4.1)
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  /*
    WiFi.begin("ssid", "password");

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
  */

  if (!SPIFFS.begin()) {
    Serial.println("Error mounting SPIFFS");
    return;
  }
  Serial.println("SPIFFS mounted");

#ifdef DS18B20
  if (!sensors.getAddress(thermo, 0)) Serial.println("Unable to find address for Device 0");
  sensors.setResolution(thermo, 10);
  Serial.println("sensor configured");
#endif

  webServer.on("/gauge.min.js",  handleGaugeScript);
  webServer.on("/Chart.min.css", handleChartStyle);
  webServer.on("/Chart.min.js.gz", handleScript);
  webServer.on("/style.css", handleStyle);
  webServer.onNotFound(handleRoot);
  webServer.begin();

  webSocket.onEvent(webSocketEvent);
  webSocket.begin();

  Serial.println("Starting servers");
}


void loop() {
  dnsServer.processNextRequest(); // captive portal support
  webServer.handleClient();
  webSocket.loop();
}
