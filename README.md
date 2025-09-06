
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
### vscode platformio.ini example

<code>
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
board = esp32s3-n16r8-USBOTG
framework = 
	arduino
	espidf
 lib_deps = 
	NewNimBLE-esp32
</code>

# platform.io Kconfig
Kconfig is used by the platform.io menuconfig (accessed by running: pio run -t menuconfig) to interactively manage the various #ifdef statements throughout the espidf and supporting libraries. The menuconfig process generates the sdkconfig file which is ultimately used behind the scenes by espidf compile+build process.

Make sure to append or symlink this Kconfig content in lib/NewNimBLE-esp32 into the Kconfig.project file locted in the /src folder of your project.

You symlink (or copy) the included Kconfig into your platform.io projects src directory. The file should be named Kconfig.projbuild in your projects src\ directory

 Once  Kconfig.projbuild is working then you will be able to choose the configurations according to your setup or the NewNimBLE-esp32 libraries will be compiled. Although you might also need to delete your .pio/build directory before the options appear .. again, the pio run -t menuconfig doens't always notice the new Kconfig files!

menuconfig will show on the main first screen as "NimBLE-esp32 configuration  --->     " line


# Using 
This library is intended to be compatible with the original ESP32 BLE functions and types with minor changes.  

If you have not used the original Bluedroid library please refer to the [New user guide](docs/New_user_guide.md).  

If you are familiar with the original library, see: [The migration guide](docs/Migration_guide.md) for details about breaking changes and migration.  

Also see [Improvements_and_updates](docs/Improvements_and_updates.md) for information about non-breaking changes.  

[Full API documentation and class list can be found here.](https://h2zero.github.io/esp-nimble-cpp/)  
<br/>  




