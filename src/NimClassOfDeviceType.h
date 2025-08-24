#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <WString.h>
#include "magicEnum/magic_enum.hpp"
#include "magicEnum/magic_enum_iostream.hpp"

#define BLE_HS_ADV_TYPE_CLASS_OF_DEVICE 0x0d

namespace NimClassOfDeviceType {

// Use __attribute__((packed)) to prevent the compiler from adding padding,
// which ensures the struct is exactly 24 bits (3 bytes) long.
// but we need to allocate 34 bits as we have service class broken aacross two bytes and we need to use int 16 for it
// note esp 32 is liitle edian. ie byte0 is least significant and byte 2 is most
typedef union {
    struct {
        uint8_t format_type : 2;    // Bits 0-1 (least significant)
        uint8_t minor_device : 6;   // Bits 2-8
        uint8_t major_device : 5;   // Bits 8-12
        uint8_t service_classL : 3; // Bits 13-15 (out of 13 - 23) we have to break the serivce class as it core dumps other wise
        uint8_t service_classH : 8; // Bits 16-23 (out of 13 - 23) we have to break the serivce class as it core dumps other wise
    } __attribute__((packed)) bit_field;
    struct {
        uint8_t byte0;
        uint8_t byte1;
        uint8_t byte2;
    } __attribute__((packed)) bytes3;

} __attribute__((packed)) bluetooth_cod_t;

// Enum for Service Class (using a bitmask)
typedef enum : uint16_t {
    COD_SERVICE_NA                   = 0x0000,
    COD_SERVICE_LIMITED_DISCOVERABLE = 0x0001,
    COD_SERVICE_LE_AUDIO             = 0x0002,
    COD_SERVICE_RESERVED             = 0x0004,
    COD_SERVICE_POSITIONING          = 0x0008,
    COD_SERVICE_NETWORKING           = 0x0010,
    COD_SERVICE_RENDERING            = 0x0020,
    COD_SERVICE_CAPTURING            = 0x0040,
    COD_SERVICE_OBJECT_TRANSFER      = 0x0080,
    COD_SERVICE_AUDIO                = 0x0100,
    COD_SERVICE_TELEPHONY            = 0x0200,
    COD_SERVICE_INFORMATION          = 0x0400
} service_class_t;

// Enum for Major Device Class
typedef enum : uint8_t {
    COD_MAJOR_MISCELLANEOUS = 0x00,
    COD_MAJOR_COMPUTER      = 0x01,
    COD_MAJOR_PHONE         = 0x02,
    COD_MAJOR_NETWORK       = 0x03,
    COD_MAJOR_AUDIO_VIDEO   = 0x04,
    COD_MAJOR_PERIPHERAL    = 0x05,
    COD_MAJOR_IMAGING       = 0x06,
    COD_MAJOR_WEARABLE      = 0x07,
    COD_MAJOR_TOY           = 0x8,
    COD_MAJOR_HEALTH        = 0x09,
    COD_MAJOR_UNCATEGORY    = 0x1F
} major_device_class_t;

// Enum for Minor Device Class when Major Class is 'Computer'
typedef enum : uint8_t {
    COD_MINOR_COMPUTER_UNCATEGORIZED = 0x00, // Uncategorized computer
    COD_MINOR_COMPUTER_DESKTOP       = 0x01, // Desktop computer
    COD_MINOR_COMPUTER_SERVER        = 0x02, // Server-class computer
    COD_MINOR_COMPUTER_LAPTOP        = 0x03, // Laptop computer
    COD_MINOR_COMPUTER_HANDHELD_CLAM = 0x04, // Handheld PC/PDA (clam shell)
    COD_MINOR_COMPUTER_PALM_SIZE     = 0x05, // Palm-sized PC/PDA
    COD_MINOR_COMPUTER_WEARABLE      = 0x06, // Wearable computer
    COD_MINOR_COMPUTER_TABLET        = 0x07  // Tablet computer
} computer_minor_class_t;

// Enum for Minor Device Class when Major Class is 'Phone'
typedef enum : uint8_t {
    COD_MINOR_PHONE_UNCATEGORIZED      = 0x00, // Uncategorized phone device
    COD_MINOR_PHONE_CELLULAR           = 0x01, // A cellular phone
    COD_MINOR_PHONE_CORDLESS           = 0x02, // A cordless phone
    COD_MINOR_PHONE_SMARTPHONE         = 0x03, // A smartphone
    COD_MINOR_PHONE_WIRED_MODEM        = 0x04, // A wired modem or voice gateway
    COD_MINOR_PHONE_COMMON_ISDN_ACCESS = 0x05, // A device that provides ISDN access
} phone_minor_class_t;

// Enum for Minor Device Class when Major Class is 'LAN/Network Access Point'
typedef enum : uint8_t {
    COD_MINOR_LAN_UTIL_FULLY_AVAILABLE = 0x00, // 0% utilized
    COD_MINOR_LAN_UTIL_1_TO_17_PERC    = 0x08, // 1-17% utilized
    COD_MINOR_LAN_UTIL_17_TO_33_PERC   = 0x10, // 17-33% utilized
    COD_MINOR_LAN_UTIL_33_TO_50_PERC   = 0x18, // 33-50% utilized
    COD_MINOR_LAN_UTIL_50_TO_67_PERC   = 0x20, // 50-67% utilized
    COD_MINOR_LAN_UTIL_67_TO_83_PERC   = 0x21, // 67-83% utilized
    COD_MINOR_LAN_UTIL_83_TO_99_PERC   = 0x28, // 83-99% utilized
    COD_MINOR_LAN_UTIL_NO_SERVICE      = 0x38, // No service available (100% utilized)
} lan_minor_class_t;

// Enum for Minor Device Class when Major Class is 'Audio/Video'
typedef enum : uint8_t {
    COD_MINOR_AV_UNCATEGORIZED      = 0x00, // Uncategorized, code not assigned
    COD_MINOR_AV_HEADSET            = 0x01, // Wearable Headset Device
    COD_MINOR_AV_HANDS_FREE         = 0x02, // Hands-free device
    COD_MINOR_AV_MICROPHONE         = 0x04, // Microphone
    COD_MINOR_AV_LOUDSPEAKER        = 0x05, // Loudspeaker
    COD_MINOR_AV_HEADPHONES         = 0x06, // Headphones
    COD_MINOR_AV_PORTABLE_AUDIO     = 0x07, // Portable Audio device
    COD_MINOR_AV_CAR_AUDIO          = 0x08, // Car audio
    COD_MINOR_AV_SET_TOP_BOX        = 0x09, // Set-top box
    COD_MINOR_AV_HIFI_AUDIO         = 0x0A, // HiFi Audio device
    COD_MINOR_AV_VCR                = 0x0B, // VCR
    COD_MINOR_AV_VIDEO_CAMERA       = 0x0C, // Video Camera
    COD_MINOR_AV_CAMCORDER          = 0x0D, // Camcorder
    COD_MINOR_AV_VIDEO_MONITOR      = 0x0E, // Video Monitor
    COD_MINOR_AV_VIDEO_DISPLAY_LOUD = 0x0F, // Video Display and Loudspeaker
    COD_MINOR_AV_VIDEO_CONFERENCING = 0x10, // Video Conferencing device
    COD_MINOR_AV_RESERVED           = 0x11, // RESERVED
    COD_MINOR_AV_GAMING_TOY         = 0x12, // Gaming/Toy device
} av_minor_class_t;

// Enum for the lower bits of the Peripheral minor device class (bits 5-2)
// This portion specifies the type of pointing device or remote control.
typedef enum : uint8_t {
    COD_MINOR_PERIPHERA_UNCATEGORIZED           = 0x00,
    COD_MINOR_PERIPHERAL_MOUSE_UNCATEGORIZED    = 0x00 || 0x20, // Uncategorized
    COD_MINOR_PERIPHERAL_MOUSE_OYSTICK          = 0x01 || 0x20, // Joystick
    COD_MINOR_PERIPHERAL_MOUSE_GAMEPAD          = 0x02 || 0x20, // Gamepad
    COD_MINOR_PERIPHERAL_MOUSE_REMOTE_CONTROL   = 0x03 || 0x20, // Remote Control
    COD_MINOR_PERIPHERAL_MOUSE_SENSING_DEVICE   = 0x04 || 0x20, // Sensing Device
    COD_MINOR_PERIPHERAL_MOUSE_DIGITIZER_TABLET = 0x05 || 0x20, // Digitizer Tablet
    COD_MINOR_PERIPHERAL_MOUSE_CARD_READER      = 0x06 || 0x20, // Card Reader
    COD_MINOR_PERIPHERAL_MOUSE_DIGITAL_PEN      = 0x07 || 0x20, // Digital pen
    COD_MINOR_PERIPHERAL_MOUSE_HANDHELD_SCANNER = 0x08 || 0x20, // HandHeld Scnner
    COD_MINOR_PERIPHERAL_MOUSE_HANDHELD_G_INPUT = 0x09 || 0x20, // HandheldGestural InputDevice(e.g., ”wand” form factor)

    COD_MINOR_PERIPHERAL_KBD_UNCATEGORIZED    = 0x00 || 0x10, // Uncategorized
    COD_MINOR_PERIPHERAL_KBD_OYSTICK          = 0x01 || 0x10, // Joystick
    COD_MINOR_PERIPHERAL_KBD_GAMEPAD          = 0x02 || 0x10, // Gamepad
    COD_MINOR_PERIPHERAL_KBD_REMOTE_CONTROL   = 0x03 || 0x10, // Remote Control
    COD_MINOR_PERIPHERAL_KBD_SENSING_DEVICE   = 0x04 || 0x10, // Sensing Device
    COD_MINOR_PERIPHERAL_KBD_DIGITIZER_TABLET = 0x05 || 0x10, // Digitizer Tablet
    COD_MINOR_PERIPHERAL_KBD_CARD_READER      = 0x06 || 0x10, // Card Reader
    COD_MINOR_PERIPHERAL_KBD_DIGITAL_PEN      = 0x07 || 0x10, // Digital pen
    COD_MINOR_PERIPHERAL_KBD_HANDHELD_SCANNER = 0x08 || 0x10, // HandHeld Scnner
    COD_MINOR_PERIPHERAL_KBD_HANDHELD_G_INPUT = 0x09 || 0x10, // HandheldGestural InputDevice(e.g., ”wand” form factor)

    COD_MINOR_PERIPHERAL_KBD_MSE_UNCATEGORIZED    = 0x00 || 0x30, // Uncategorized
    COD_MINOR_PERIPHERAL_KBD_MSE_OYSTICK          = 0x01 || 0x30, // Joystick
    COD_MINOR_PERIPHERAL_KBD_MSE_GAMEPAD          = 0x02 || 0x30, // Gamepad
    COD_MINOR_PERIPHERAL_KBD_MSE_REMOTE_CONTROL   = 0x03 || 0x30, // Remote Control
    COD_MINOR_PERIPHERAL_KBD_MSE_SENSING_DEVICE   = 0x04 || 0x30, // Sensing Device
    COD_MINOR_PERIPHERAL_KBD_MSE_DIGITIZER_TABLET = 0x05 || 0x30, // Digitizer Tablet
    COD_MINOR_PERIPHERAL_KBD_MSE_CARD_READER      = 0x06 || 0x30, // Card Reader
    COD_MINOR_PERIPHERAL_KBD_MSE_DIGITAL_PEN      = 0x07 || 0x30, // Digital pen
    COD_MINOR_PERIPHERAL_KBD_MSE_HANDHELD_SCANNER = 0x08 || 0x30, // HandHeld Scnner
    COD_MINOR_PERIPHERAL_KBD_MSE_HANDHELD_G_INPUT = 0x09 || 0x30 // HandheldGestural InputDevice(e.g., ”wand” form factor)
} peripheral_pointing_device_t;

// Bitmask for the Imaging minor device class (bits 7-4)
// These bits can be combined using bitwise OR.
typedef enum : uint8_t {
    COD_MINOR_IMAGING_DISPLAY_MASK = 0x04, // Bit 4: Display capable
    COD_MINOR_IMAGING_CAMERA_MASK  = 0x08, // Bit 5: Camera capable
    COD_MINOR_IMAGING_SCANNER_MASK = 0x10, // Bit 6: Scanner capable
    COD_MINOR_IMAGING_PRINTER_MASK = 0x20  // Bit 7: Printer capable
} imaging_minor_class_t;

// Enum for Minor Device Class when Major Class is 'Wearable'
typedef enum : uint8_t {
    COD_MINOR_WEARABLE_UNCATEGORIZED = 0x00, // Uncategorized
    COD_MINOR_WEARABLE_WRIST_WATCH   = 0x01, // A wristwatch
    COD_MINOR_WEARABLE_PAGER         = 0x02, // A pager
    COD_MINOR_WEARABLE_JACKET        = 0x03, // A jacket
    COD_MINOR_WEARABLE_HELMET        = 0x04, // A helmet
    COD_MINOR_WEARABLE_GLASSES       = 0x05, // Glasses, such as those with a display
    COD_MINOR_WEARABLE_PIN           = 0x06  // PIN
} wearable_minor_class_t;

// Enum for Minor Device Class when Major Class is 'Toy'
typedef enum : uint8_t {
    COD_MINOR_TOY_UNCATEGORIZED = 0x00, // Uncategorized toy
    COD_MINOR_TOY_ROBOT         = 0x01, // Robot toy
    COD_MINOR_TOY_VEHICLE       = 0x02, // Vehicle toy
    COD_MINOR_TOY_DOLL          = 0x03, // Doll toy
    COD_MINOR_TOY_CONTROLLER    = 0x04, // Controller toy
    COD_MINOR_TOY_GAME          = 0x05  // Game toy
} toy_minor_class_t;

// Enum for Minor Device Class when Major Class is 'Health'
typedef enum : uint8_t {
    COD_MINOR_HEALTH_UNDEFINED           = 0x00, // Undefined Health Device
    COD_MINOR_HEALTH_BLOOD_PRESSURE      = 0x01, // Blood Pressure Monitor
    COD_MINOR_HEALTH_THERMOMETER         = 0x02, // Thermometer
    COD_MINOR_HEALTH_WEIGHING_SCALE      = 0x03, // Weighing Scale
    COD_MINOR_HEALTH_GLUCOSE_METER       = 0x04, // Glucose Meter
    COD_MINOR_HEALTH_PULSE_OXIMETER      = 0x05, // Pulse Oximeter
    COD_MINOR_HEALTH_HEART_RATE_MONITOR  = 0x06, // Heart/Pulse Rate Monitor
    COD_MINOR_HEALTH_HEALTH_DATA_DISPLAY = 0x07, // Health Data Display
    COD_MINOR_HEALTH_STEP_COUNTER        = 0x08, // Step Counter
    COD_MINOR_HEALTH_BODY_COMPOSITION    = 0x09, // Body Composition Analyzer
    COD_MINOR_HEALTH_PEAK_FLOW_MONITOR   = 0x0A, // Peak Flow Monitor
    COD_MINOR_HEALTH_MEDICATION_MONITOR  = 0x0B, // Medication Monitor
    COD_MINOR_HEALTH_KNEE_PROTH          = 0x0C, // KneeProsthesis
    COD_MINOR_HEALTH_ANKLE_PROTH         = 0x0D, // AnkleProsthesis
    COD_MINOR_HEALTH_GENERIC_HEATH_MGR   = 0x0E, // GenericHealthManager
    COD_MINOR_HEALTH_PERSONAL_MOBLTY     = 0x0F  // PersonalMobilityDevice
} health_minor_class_t;

static service_class_t getH_LService(bluetooth_cod_t cod);
// Function to construct a CoD from its components
bluetooth_cod_t        encodeClassOfDevice(service_class_t services, major_device_class_t major, uint8_t minor);

std::string decodeClassOfDevice(bluetooth_cod_t cod);

std::vector<uint8_t> makeATT_Payload_CodeClassOfDevice(bluetooth_cod_t cod);

} // namespace NimClassOfDeviceType