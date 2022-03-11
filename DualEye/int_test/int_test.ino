#include <ArduinoSort.h> // https://github.com/emilv/ArduinoSort
#include <Ticker.h>      // https://github.com/esp8266/Arduino/tree/master/libraries/Ticker
Ticker timer;

#define POWER_STROKES_PER_REVOLUTION 2  // for air cooled 4 cy VW

byte tachoPin  = D5;  // on esp8266, GPIO 14, tachometer input
volatile unsigned long count;     
volatile unsigned long rpm;
volatile unsigned long finalrpm;

// tachometer isr
inline void ICACHE_RAM_ATTR pulseISR()
{
  count++;
}


void runTick() {
 rpm = (count * 60) / POWER_STROKES_PER_REVOLUTION;
 count = 0; 
 finalrpm = (rpm + (3 * finalrpm)) / 4;  // do some crude averaging
}


void setup() {
  Serial.begin(115200);
  Serial.println("started");
  pinMode(tachoPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(tachoPin), pulseISR, RISING);
  timer.attach(1, runTick); // start sending data on the web socket once a sec
}

void loop() {

 delay(1000);
 //finalrpm = (rpm + (3 * finalrpm)) / 4;  // do some crude averaging
 Serial.printf("rpm %d\n", rpm);
}
