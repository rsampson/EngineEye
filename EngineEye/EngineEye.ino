#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>

// Captive portal code from: https://github.com/esp8266/Arduino/blob/master/libraries/DNSServer/examples/CaptivePortal/CaptivePortal.ino
const byte DNS_PORT = 53;
IPAddress apIP(172, 217, 28, 1);
DNSServer dnsServer;
ESP8266WebServer webServer(80);

#ifdef USE_THERMOCOUPLE
// if using a thermocouple, you may want to consider using the MAX31856 module 
// (library at https://github.com/adafruit/Adafruit_MAX31856) instead of the older max6675
#include "max6675.h" // www.ladyada.net/learn/sensors/thermocouple

// must set board to wemos d1 mini or similar in order to use these pin definitions
#define thermoDO D6
#define thermoCS_1 D7
#define thermoCS_2 D4
#define thermoCLK D8

MAX6675 thermocouple_1(thermoCLK, thermoCS_1, thermoDO);
MAX6675 thermocouple_2(thermoCLK, thermoCS_2, thermoDO);
#endif


float tempC_1 = 100;  // put in some dummy values for test
float tempF_1 = 200;
float tempC_2 = 300;
float tempF_2 = 400;
float voltage = 500;

 char rootPage[] = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta http-equiv='refresh' content='3'/> 
    <title>Engine Temperature</title>
    <style>
      body { background-color: white; 
             font-family: Sans-Serif;  
           }
      h1 { 
            font-size : 6em;
            padding : 10px ;
            padding-left : 10px;
            width: auto;
            border: 10px solid lightgrey;
            border-radius: 20px;
            margin: 10px ;
            color: green;
         } 
     </style>
    <script type="text/javascript" >
    
    function hotText(elemID, level){
          var tempelem = document.getElementById(elemID);
          var tempnum = parseInt(tempelem.innerHTML, 10);

          if (tempnum > level) {
            tempelem.style.color = "red";
         }
         else {
         tempelem.style.color = "green";  
         }
       }

    </script>
   </head>
  <body>
    <h1> <span id="temp1C">%4.1f</span>C  <span id="temp1F">%4.1f</span>F </h1>
    <h1> <span id="temp2C">%4.1f</span>C  <span id="temp2F">%4.1f</span>F </h1>
    <h1> <span id="voltage">%2.2f</span>V</h1>    
    <script>
       hotText("temp1C", 121);
       hotText("temp1F", 250);
       hotText("temp2C", 204);
       hotText("temp2F", 400);
    </script> 
  </body>
</html>
)=====";

void handleRoot() {
  char temp[strlen(rootPage) + 10];

  snprintf ( temp, strlen(rootPage) + 10,
    rootPage,
    tempC_1, tempF_1, tempC_2, tempF_2, voltage
  );
  webServer.send ( 200, "text/html", temp );
}

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println();
    Serial.print("Configuring access point...");
   
    WiFi.disconnect(); 
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP("engine");

    // if DNSServer is started with "*" for domain name, it will reply with
    // provided IP to all DNS request
    dnsServer.start(DNS_PORT, "*", apIP);

    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);
    
    // replay to all requests with same HTML
    webServer.onNotFound(handleRoot);
    webServer.begin();
    
    Serial.println("HTTP server started");
}

void loop() {
   dnsServer.processNextRequest();
   webServer.handleClient();
   
#ifdef USE_THERMOCOUPLE
   tempC_1 = thermocouple_1.readCelsius();
   //tempC_1 = (41 * tempC_1) /43;  // fudge factor to use type T thermocouple if needed
   delay(250);
   tempC_2 = thermocouple_2.readCelsius();
   //tempC_2 = (41 * tempC_2) /43;  // fudge factor to use type T thermocouple if needed
   delay(250);
#endif
   
   // do the fahrenheit conversions
   tempF_1 = (tempC_1 * 9.0/5.0)+ 32;
   tempF_2 = (tempC_2 * 9.0/5.0)+ 32;
    
   //voltage =  analogRead(A0) * .01984;  // if analog input pin is available, can read bat voltage
   
   //WiFi.printDiag(Serial);
}
