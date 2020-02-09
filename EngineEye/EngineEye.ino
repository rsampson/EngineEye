#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h> 
#include <ESP8266WebServer.h>

#include "max6675.h" // www.ladyada.net/learn/sensors/thermocouple

#define thermoDO D6
#define thermoCS_1 D7
#define thermoCS_2 D4
#define thermoCLK D8
#define vccPin D5

MAX6675 thermocouple_1(thermoCLK, thermoCS_1, thermoDO);
MAX6675 thermocouple_2(thermoCLK, thermoCS_2, thermoDO);

/* Set these to your desired credentials. */
const char *ssid = "engineTemp";
const char *password = "aircooled";

float tempC_1;
float tempF_1;
float tempC_2;
float tempF_2;
float voltage;

ESP8266WebServer server(80);

/* Go to http://192.168.4.1 in a web browser
 * connected to this access point to see it.
 */

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
  server.send ( 200, "text/html", temp );
}

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println();
    Serial.print("Configuring access point...");

   
    WiFi.disconnect(); 
    WiFi.mode(WIFI_AP);
    /* You can remove the password parameter if you want the AP to be open. */
    WiFi.softAP(ssid);

    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);
    server.on("/", handleRoot);
    server.begin();
    Serial.println("HTTP server started");
    
    pinMode(vccPin, OUTPUT); digitalWrite(vccPin, HIGH);

    while (!MDNS.begin("esp8266")) {
      Serial.print(".");
      delay(500);
    }
   Serial.println("mDNS responder started");
  
   MDNS.addService("http", "tcp", 80);
}

void loop() {

   server.handleClient();
   
   //digitalWrite(vccPin, HIGH);  // uncomment if you want blinking lights!
   // no more than 4 reads per second
   // delay(250);

   tempC_1 = thermocouple_1.readCelsius();
   tempC_1 = (41 * tempC_1) /43;  // rescale to use type T thermocouple
   tempF_1 = (tempC_1 * 9.0/5.0)+ 32;
   
   delay(250);
   
   tempC_2 = thermocouple_2.readCelsius();
   tempC_2 = (41 * tempC_2) /43;  // rescale to use type T thermocouple
   tempF_2 = (tempC_2 * 9.0/5.0)+ 32;  
   
  // digitalWrite(vccPin, LOW);
   delay(250);

   voltage =  analogRead(A0) * .01984;
   
   WiFi.printDiag(Serial);
}
