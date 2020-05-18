# *** UPDATE ***
**Breaking change:** Client and scan now use `std::vector` instead of `std::map` for storing the remote attribute database.   
   
This change will affect your application code if you use `NimBLEClient::getServices()` or `NimBLERemoteService::getCharacteristics()`   
in your application as they now return a pointer to `std::vector` of the respective attributes.   

In addition `NimBLERemoteService::getCharacteristicsByHandle()` has been removed as it is no longer maintained in the library.

These changes were necessary due to the amount of resources required to use `std::map`, it was not justifed by any benfit it provided.   
   
It is expected that there will be minimal impact on most applications, if you need help adjusting your code please create an issue.  

# esp-nimble-cpp
NimBLE CPP library for use with ESP32 that attempts to maintain compatibility with the @nkolban cpp_uitls API.

Why? Because the Bluedroid library is too bulky. 

Initial client code testing has resulted in code size reduction of ~115k and reduced ram consumption of ~37k.

  
# Installation:

Download as .zip and extract to components folder in your esp-idf project.

Run menuconfig, go to `Component config->Bluetooth->` enable Bluetooth and select NimBLE host.

`#include "NimBLEDevice.h"` in main.cpp.


# Usage: 

This library is intended to be compatible with the original ESP32 BLE functions and types with minor changes.  

Check [API_DIFFERENCES](https://github.com/h2zero/esp-nimble-cpp/blob/master/API_DIFFERENCES.md) for details.


# Acknowledgments:

* @nkolban and @chegewara for the [original esp32 BLE library](https://github.com/nkolban/esp32-snippets) this project was derived from.
* @beegee-tokyo for contributing your time to test/debug and contributing the beacon examples.


# Todo:

1. Code cleanup.
2. Create documentation.
3. Expose more NimBLE features.
4. Add BLE Mesh code.