# ESP-NTP-MAX7219-MQTT

This project uses an ESP8266 to connect to our local NTP server to display the time on a MAX7219 8x8x4 LED Matrix. 

In addition, the ESP8266 subscribes to an MQTT topic and scrolls messages. I schedule messages from Home Assistant - they run on pre-defined times and give info like weather & 3D printer status. Demo gif is below. 

You will need to insert an NTP server, SSID/Password, MQTT Server, User & Password, plus check the number of MAX7219 displays is correct. 

## Display rotation: 

If your displays are upside down, see issue [#1]. 

This code may help you: 

```
matrix.setPosition(0, 3, 0); // (Display-Nr ab Einspeisung, X, Y)
matrix.setPosition(1, 2, 0);
matrix.setPosition(2, 1, 0);
matrix.setPosition(3, 0, 0);

matrix.fillScreen(0);

matrix.setIntensity(0); // Use a value between 0 and 15 for brightness
matrix.setRotation(0, 3); // The first display is position upside down
matrix.setRotation(1, 3); // The first display is position upside down
matrix.setRotation(2, 3); // The first display is position upside down
matrix.setRotation(3, 3); // The first display is position upside down
display_message("Ready");
```


## Notes:
1. This device does not have a real time clock (RTC), thus polls the NTP server a lot. If you wish to build this device, I recommend you build a small NTP server for your network to lower the load on other pools. This can be 
achieved with things like Chrony, which run in docker containers.
2. A number of other libraries are required. These are below!

## Libraries:
1. NTPtimeESP: https://github.com/SensorsIot/NTPtimeESP
2. PubSubClient: https://github.com/knolleary/pubsubclient
3. Adafruit-GFX: https://github.com/adafruit/Adafruit-GFX-Library
4. Max72xxPanel: https://github.com/markruys/arduino-Max72xxPanel

## Demo:
![MQTT-Gif](MQTT-Demo.gif)
