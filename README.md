# ardunio-remote

Code that drives an Adafruit RP2040 USB Host with Airlift Featherwing (or ESP32-S3 Feather with MAX3421E USB feather) that takes input from an HID keyboard (most likely one of those remotes with a little USB key reciever) and sends it on to Home Assistant so you can control actions based on remote clicks.

There are two versions of this, one for an [Adafruit RP2040 USB Host](https://www.adafruit.com/product/5723) with an [Adafruit Airlift Featherwing](https://www.adafruit.com/product/4264), and one for an [Adafruit ESP32-S3 Feather](https://www.adafruit.com/product/5477) with an [Adafruit USB Host FeatherWing with MAX3421E board](https://www.adafruit.com/product/5858).  The ESP32-S3 version only works for about 10 minutes before the USB Host stops responding.  I have no idea why, and the RP2040 has been rock solid, so I stopped trying to figure it out.  The code is included here in case anyone wants to take more of a look at it, but all the instructions on the wiki regarding compiling are for the RP2040.

More information on the [wiki for this repo](https://github.com/pkscout/ardunio-remote/wiki).