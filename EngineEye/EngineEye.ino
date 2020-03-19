// Code repository at: https://github.com/rsampson/EngineEye
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>

// Captive portal code from: https://github.com/esp8266/Arduino/blob/master/libraries/DNSServer/examples/CaptivePortal/CaptivePortal.ino
const byte DNS_PORT = 53;
//IPAddress apIP(172, 217, 28, 1);
DNSServer dnsServer;
ESP8266WebServer webServer(80); 

byte tachoPin  = D5;       // D5 on esp8266, GPIO ?, tachometer input
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


float tempF_1 = 0;
float tempF_2 = 0;
float voltage = 0;
float revs = 0;

 char rootPage[] = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta http-equiv='refresh' content='3'/> 
    <title>Engine Parameters</title>
    <style>
      body { background-color: white; 
             font-family: Sans-Serif;  
           }
      h1 { 
            font-size : 4em;
            padding : 10px ;
            padding-left : 10px;
            width: auto;
            border: 10px solid lightgrey;
            border-radius: 20px;
            margin: 10px ;
            color: black;
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
         tempelem.style.color = "black";  
         }
       }

    </script>
   </head>
  <body>
    <h1> <span id="temp1F">%4.1f</span> F </h1>
    <h1> <span id="temp2F">%4.1f</span> F </h1>
    <h1> <span id="voltage">%2.2f</span> V</h1>    
    <h1> <span id="revs">%4.0f</span> RPM</h1>  
    <script>
       hotText("temp1F", 250);
       hotText("temp2F", 250);
       hotText("voltage", 14.5);
       hotText("revs", 4100);
    </script> 
  </body>
</html>
)=====";


void handleRoot() {

  char temp[strlen(rootPage) + 10];

  snprintf ( temp, strlen(rootPage) + 10,   // works just like printf formating, following 
    rootPage,                      // parameters get inserted into rootpage at % markers
    tempF_1, tempF_2, voltage, revs
  );
  webServer.send ( 200, "text/html", temp );
}

void setup() {
    //Serial.begin(115200);
    //delay(100);
    
    // config tachometer
    pinMode(tachoPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(tachoPin), pulseISR, RISING);
    period = 0;
    prev = 0;
    lastDisplay = 0;

    WiFi.mode(WIFI_AP);
    //WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP("engine");

    // if DNSServer is started with "*" for domain name, it will reply with
    // provided IP to all DNS request
    //dnsServer.start(DNS_PORT, "*", apIP);
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    
    //IPAddress myIP = WiFi.softAPIP();
    //Serial.print("AP IP address: ");
    //Serial.println(myIP);

    // replay to all requests with same HTML
    webServer.onNotFound(handleRoot);
    webServer.begin();
}

void loop() {
   // captive portal support
   dnsServer.processNextRequest();
   webServer.handleClient();

#ifdef USE_THERMOCOUPLE
   tempF_1 = thermocouple_1.readFahrenheit();
   tempF_1 = (41 * tempF_1) /43;  // fudge factor to use type T thermocouple if needed
   delay(250);
   tempF_2 = thermocouple_2.readFahrenheit();
   tempF_2 = (41 * tempF_2) /43;  // fudge factor to use type T thermocouple if needed
   delay(250);
#endif
   // reading the analog port causes the system to lock up.
   //voltage =  analogRead(A0) * .01984;  // if analog input pin is available, can read bat voltage
   
   // tachometer processing, each 250 ms
  if( (millis() - lastDisplay) >= 250 )
  {
    lastDisplay = millis();
    if(period!=0)
      rpm = (unsigned int) 60000000/period;
    else
      rpm = 0;
  }
  revs = rpm;
    
   //WiFi.printDiag(Serial);
 }
