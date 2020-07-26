# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(SUPPORTED_TARGETS esp32)

idf_component_register(SRCS "src/FreeRTOS.cpp"
                            "src/NimBLE2904.cpp"
                            "src/NimBLEAddress.cpp"
                            "src/NimBLEAdvertisedDevice.cpp"
                            "src/NimBLEAdvertising.cpp"
                            "src/NimBLEBeacon.cpp"
                            "src/NimBLECharacteristic.cpp"
                            "src/NimBLEClient.cpp"
                            "src/NimBLEDescriptor.cpp"
                            "src/NimBLEDevice.cpp"
                            "src/NimBLEEddystoneTLM.cpp"
                            "src/NimBLEEddystoneURL.cpp"
                            "src/NimBLERemoteCharacteristic.cpp"
                            "src/NimBLERemoteDescriptor.cpp"
                            "src/NimBLERemoteService.cpp"
                            "src/NimBLEScan.cpp"
                            "src/NimBLESecurity.cpp"
                            "src/NimBLEServer.cpp"
                            "src/NimBLEService.cpp"
                            "src/NimBLEUtils.cpp"
                            "src/NimBLEUUID.cpp"
                  INCLUDE_DIRS "src"
                  REQUIRES bt)
