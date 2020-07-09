# esp-nimble-cpp

NimBLE CPP library for use with ESP32 that attempts to maintain compatibility with the [nkolban cpp_uitls BLE API](https://github.com/nkolban/esp32-snippets/tree/master/cpp_utils).

**An Arduino version of this library, including NimBLE, can be [found here.](https://github.com/h2zero/NimBLE-Arduino)**

This library **significantly** reduces resource usage and improves performance for ESP32 BLE applications as compared    
with the bluedroid based library. The goal is to maintain, as much as reasonable, compatibility with the original   
library but refactored to use the NimBLE stack. In addition, this library will be more actively developed and maintained   
to provide improved capabilites and stability over the original.

**Testing shows a nearly 50% reduction in flash use and approx. 100kB less ram consumed vs the original!**  
*Your results may vary*  
<br/>

### What is NimBLE?
NimBLE is a completely open source Bluetooth Low Energy stack produced by [Apache](https://github.com/apache/mynewt-nimble).  
It is more suited to resource constrained devices than bluedroid and has now been ported to the ESP32 by Espressif.  
<br/>

# Installation

### ESP-IDF v4.0+
Download as .zip and extract or clone into the components folder in your esp-idf project.

Run menuconfig, go to `Component config->Bluetooth` enable Bluetooth and in `Bluetooth host` NimBLE.  
Configure settings in `NimBLE Options`.  
`#include "NimBLEDevice.h"` in main.cpp.  
Call `NimBLEDevice::init("");` in `app_main`.  
<br/>

### ESP-IDF v3.2 & v3.3
The NimBLE component does not come with these versions of IDF.  
A backport that works in these versions has been created and is [available here](https://github.com/h2zero/esp-nimble-component).  
Download or clone that repo into your project/components folder and run menuconfig.
Configure settings in `main menu -> NimBLE Options`.  

`#include "NimBLEDevice.h"` in main.cpp.  
Call `NimBLEDevice::init("");` in `app_main`.  
<br/>  

# Using
This library is intended to be compatible with the original ESP32 BLE functions and types with minor changes.  

See: [Breaking API Changes vs Original](docs/BREAKING_API_CHANGES.md) for details.  

Also see [Improvements_and_updates](docs/Improvements_and_updates.md) for information about non-breaking changes.  
<br/>  

# Acknowledgments
* [nkolban](https://github.com/nkolban) and [chegewara](https://github.com/chegewara) for the [original esp32 BLE library](https://github.com/nkolban/esp32-snippets/tree/master/cpp_utils) this project was derived from.
* [beegee-tokyo](https://github.com/beegee-tokyo) for contributing your time to test/debug and contributing the beacon examples.
* [Jeroen88](https://github.com/Jeroen88) for the amazing help debugging and improving the client code.  
<br/>  

# Todo
1. Implement random addresses.
2. Implement NimBLEServer::removeService
3. Implement extra fields in NimBLEAdvertisedDevice.
4. Document nimconfig.
5. Add BLE Mesh code.  
<br/>  

