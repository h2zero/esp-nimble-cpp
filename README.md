# *** UPDATES ***
**Breaking changes:**   
**NEW** on June 21, 2020
> ```
> NimBLEClient::getServices(bool refresh = false)   
> NimBLERemoteService::getCharacteristics(bool refresh = false)   
> NimBLERemoteCharacteristic::getDecriptors(bool refresh = false)
>```
These methods now take an optional (bool) parameter.   
If true it will clear the respective vector and retrieve all the respective attributes from the peripheral.   
If false(default) it will return the respective vector empty or otherwise with the currently stored attributes. 

**NEW** on May 23, 2020   
Client and scan now use `std::vector` instead of `std::map` for storing the remote attribute database.   
   
This change will affect your application code if you use `NimBLEClient::getServices()` or `NimBLERemoteService::getCharacteristics()`   
in your application as they now return a pointer to `std::vector` of the respective attributes.   

In addition `NimBLERemoteService::getCharacteristicsByHandle()` has been removed as it is no longer maintained in the library.

These changes were necessary due to the amount of resources required to use `std::map`, it was not justifed by any benfit it provided.   
   
It is expected that there will be minimal impact on most applications, if you need help adjusting your code please create an issue.   

# esp-nimble-cpp
NimBLE CPP library for use with ESP32 that attempts to maintain compatibility with the @nkolban cpp_uitls API.

This library **significantly** reduces resource usage and improves performance for ESP32 BLE applications as compared    
with the bluedroid based library. The goal is to maintain, as much as reasonable, compatibility with the original   
library but refactored to use the NimBLE stack. In addition, this library will be more actively developed and maintained   
to provide improved capabilites and stability over the original.

**Testing shows a nearly 50% reduction in flash use and approx. 100kB less ram consumed vs the original!**

  
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
* @Jeroen88 for the amazing help debugging and improving the client code.

# Todo:

1. Create documentation.
2. Add BLE Mesh code.
3. Expose more NimBLE features.