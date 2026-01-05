# ESP-CAN

An ESP CAN TX/RX monitor that can be wired to can network to read messages on the line or send message to the line 

## Hardware 
[ESP32-S3-Touch-LCD-4.3](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-4.3)

## To customize button on the ui
Just change the `` s_buttons[] `` array in `` main/src/ui_canmon.c ``


## Requirements
- [ESP-IDF](http://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/linux-macos-setup.html#get-started-get-esp-idf) is required

## Quick Start
```bash
$ git clone https://github.com/BG4zi/esp-can.git
$ cd esp-can
$ idf.py reconfigure
$ idf.py build flash monitor
```
