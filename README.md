# *** UPDATE ***

This library is now ready with (mostly) all original BLE library compatiblity.   
Check the examples and API_DIFFERENCES document for details of using this library.
 
3 simultaneous connections tested stable so far on both client and server.


# esp-nimble-cpp
A fork of the NimBLE stack restructured for compilation in the Ardruino IDE with a CPP library for use with ESP32.

Why? Because the Bluedroid library is too bulky. 

Initial client code testing has resulted in code size reduction of ~115k and reduced ram consumption of ~37k.

  
# Installation:

Download as .zip and extract to components folder in your esp-idf project.

`#include "NimBLEDevice.h"` in main.cpp.


# Usage: 

This library is intended to be compatible with the original ESP32 BLE functions and types with minor changes.


# Acknowledgments:

* @nkolban and @chegewara for the [original esp32 BLE library](https://github.com/nkolban/esp32-snippets) this project was derived from.
* @beegee-tokyo for contributing your time to test/debug and contributing the beacon examples.


# Todo:

1. Code cleanup.
2. Create documentation.
3. Expose more NimBLE features.
4. Add BLE Mesh code.

