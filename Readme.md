## ESP32 and ESP8266 fake windows xp port

This is a Wifi Radio project made by [VolosR](https://github.com/VolosR/TTGOInternetStation), based on [Espressif mp3 library](https://github.com/espressif/ESP8266_MP3_DECODER) that loads and play internet audio streams, like Radio stations.

I changed the structure of the project to be run as Platformio project, because
- I want to run is on both esp8266 and esp32
- I want to run it with multiple screen options
 
### Why

I'm working on my own development boards for both listed MCUs and this project is a good demonstration of kind of projects they would happily support

### Demo

#### ESP32

![esp32](/doc/demo/VID_20211223_214636.gif)

#### ESP8266

![esp8266](/doc/demo/VID_20211223_220631.gif)