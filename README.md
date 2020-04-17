# EngineEye
System for reading auto engine parameters (I will start with oil temperature and RPM) over wifi using a cell phone.

This has been created for use with 4 cylinder aircooled VW engines, but could be modified for use with other vehicles and could be expanded to measure cylinder head temperature, oil pressure, manifold vacuum, etc. 

To build: 

1)Download and install the Arduino IDE: https://www.arduino.cc/en/main/software

2)Install all required libraries: https://learn.sparkfun.com/tutorials/installing-an-arduino-library/all  

3)Upload the content of the data directory to the SPIFFS: https://www.instructables.com/id/Using-ESP8266-SPIFFS/

To make it easier to use your phone or tablet while driving, Android has a setting to keep the screen on indefinitely, but only if it is plugged in and charging. 

1) First enter Developer Mode (tap the Build number entry in the About phone menu in Settings seven times).

2)Choose System->Advanced->Developer options from the Settings menu. 

3)Choose Stay awake and as long as your phone or tablet is plugged in.

Disclaimer:  You can start a fire in your engine with a short circuit, get wires tangled in the fan belt, and generally foobar your car with this if you're not careful. Don't blame me if your priceless classic vehicle catches fire and throws a rod wile using this code. Mess with this stuff at your own risk!
