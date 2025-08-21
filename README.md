
# esp-nimble-cpp


Bluetooth low energy (BLE) library for Arduino based on NimBLE. this is based on https://github.com/h2zero/esp-nimble-cpp but it is updated to work with #ArduinoIoT and also with esp32 #Arduino 3.3.0 core

# What is NimBLE?
NimBLE is a completely open source Bluetooth Low Energy stack produced by [Apache](https://github.com/apache/mynewt-nimble).  
It is more suited to resource constrained devices than bluedroid and has now been ported to the ESP32 by Espressif.  
<br/>

## Using with Arduino as an IDF component and CMake
When using this library along with Arduino and compiling with *CMake* you must add `add_compile_definitions(ARDUINO_ARCH_ESP32=1)`  
in your project/CMakeLists.txt after the line `include($ENV{IDF_PATH}/tools/cmake/project.cmake)` to prevent Arduino from releasing BLE memory.
<br>

# Installation

### ESP-IDF v4.0+
Download as .zip and extract or clone into the components folder in your esp-idf project.

Run menuconfig, go to `Component config->Bluetooth` enable Bluetooth and in `Bluetooth host` NimBLE.  
Configure settings in `NimBLE Options`.  
`#include "NimBLEDevice.h"` in main.cpp.  
Call `NimBLEDevice::init("");` in `app_main`.  
<br/>

# Using 
This library is intended to be compatible with the original ESP32 BLE functions and types with minor changes.  

If you have not used the original Bluedroid library please refer to the [New user guide](docs/New_user_guide.md).  

If you are familiar with the original library, see: [The migration guide](docs/Migration_guide.md) for details about breaking changes and migration.  

Also see [Improvements_and_updates](docs/Improvements_and_updates.md) for information about non-breaking changes.  

[Full API documentation and class list can be found here.](https://h2zero.github.io/esp-nimble-cpp/)  
<br/>  




