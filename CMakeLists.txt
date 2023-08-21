# get IDF version for comparison
set(idf_version "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}")

set(COMPONENT_REQUIRES driver)

# set driver sources only for supported platforms
if(IDF_TARGET STREQUAL "esp32" OR IDF_TARGET STREQUAL "esp32s3" OR IDF_TARGET STREQUAL "esp32c2" OR IDF_TARGET STREQUAL "esp32c3" OR IDF_TARGET STREQUAL "esp32c6")
  set(COMPONENT_SRCS
    src/NimBLE2904.cpp
    src/NimBLEAddress.cpp
    src/NimBLEAdvertisedDevice.cpp
    src/NimBLEAdvertising.cpp
    src/NimBLEBeacon.cpp
    src/NimBLECharacteristic.cpp
    src/NimBLEClient.cpp
    src/NimBLEDescriptor.cpp
    src/NimBLEDevice.cpp
    src/NimBLEEddystoneTLM.cpp
    src/NimBLEEddystoneURL.cpp
    src/NimBLEExtAdvertising.cpp
    src/NimBLEHIDDevice.cpp
    src/NimBLERemoteCharacteristic.cpp
    src/NimBLERemoteDescriptor.cpp
    src/NimBLERemoteService.cpp
    src/NimBLEScan.cpp
    src/NimBLEServer.cpp
    src/NimBLEService.cpp
    src/NimBLEUtils.cpp
    src/NimBLEUUID.cpp
    )

  set(COMPONENT_ADD_INCLUDEDIRS
    src
    )

  set(COMPONENT_PRIV_REQUIRES bt nvs_flash)

endif()

register_component()
