#!/bin/sh
#

avrdude -c arduino -p atmega328p -P /dev/ttyUSB0 -b 115200 -U flash:w:build/ArduinoProject.hex:i
