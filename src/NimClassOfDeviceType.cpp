
#include "NimBLELog.h"
#include <HEXBuilder.h>

#if defined(CONFIG_NIMBLE_USE_MAGIC_ENUM)
# include "magicEnum/magic_enum.hpp"
# include "magicEnum/magic_enum_iostream.hpp"

template <typename E>
auto to_integer(magic_enum::Enum<E> value) -> int
{
  // magic_enum::Enum<E> - C++17 Concept for enum type.
  return static_cast<magic_enum::underlying_type_t<E>>(value);
}

#endif
#include "NimClassOfDeviceType.h"

#define BLE_HS_ADV_TYPE_CLASS_OF_DEV 0x0d

static const char* LOG_TAG = "NimClassOfDeviceType";

namespace NimClassOfDeviceType {

// fucntion to add the serivce H aand L comps togetther
static NimClassOfDeviceType::service_class_t getH_LService(bluetooth_cod_t cod) {
    uint16_t service_highBit = (cod.bit_field.service_classH << 3);
    uint16_t service         = service_highBit | ((uint16_t)cod.bit_field.service_classL);
    NIMBLE_LOGI(LOG_TAG,
                " Merging Service H (0x%02x) and L (0x%02x) into (0x%02x) ",
                service_highBit,
                cod.bit_field.service_classL,
                service);

    return (NimClassOfDeviceType::service_class_t)service;
}

// Function to construct a CoD from its components
bluetooth_cod_t encodeClassOfDevice(service_class_t services, major_device_class_t major, uint8_t minor) {
    bluetooth_cod_t cod        = {0x000000};   // Initialize all bits to zero
    cod.bit_field.format_type  = 0;            // Standard format
    cod.bit_field.major_device = major & 0x1f; // on;u use the 5 lower bits
    cod.bit_field.minor_device = minor & 0x3f; // only use the 6 lower bits
    if (services > 0x007) {
        // any bigger we get a core dump so we need to manually break the service setting.
        uint8_t L_3Bit               = services & 0x0007; // only use the 3 lower bits
        cod.bit_field.service_classL = L_3Bit;
        uint8_t H_8Bit =
            (uint8_t)((services >> 3) &
                      0x00FF);     // get the remaining 8 bits and shift left by 3 for deleting the 3 bits to discard
        cod.bytes3.byte2 = H_8Bit; // stuff the reming 8 bits in the byte 2
        NIMBLE_LOGI(LOG_TAG, "Breaking services 0x%02x into 3 bits 0x%02x and 8 bits 0x%02x", services, L_3Bit, H_8Bit);
    } else {
        // increasing bits from 11 to 16 as the esp32 core dumps at higher bits ( this filed (two bytes)is now across three bytes !)
        cod.bit_field.service_classL = services & 0x007; // only use the 3 lower bits
    }

    NIMBLE_LOGI(LOG_TAG,
                "Making servies %s RAW HEX cod value only three bytes are used %s",
                decodeClassOfDevice(cod).c_str(),
                HEXBuilder::bytes2hex((((const unsigned char*)&cod)), 3).c_str());

    NIMBLE_LOGI(LOG_TAG,
                " Making COB Byte0:0x%02x Byte1:0x%02x Byte2:0x%02x",
                (cod.bytes3.byte0),
                (cod.bytes3.byte1),
                (cod.bytes3.byte2));
    return cod;
}

std::string decodeMajorMinor(major_device_class_t major, uint8_t minor) {
    std::string ret = "Magic Enum not enabled";
#if defined(CONFIG_NIMBLE_USE_MAGIC_ENUM)
    std::string cmd = "";
    ret             = " Major Device:";
    cmd             = ((magic_enum::enum_flags_name(major)));
    if (major == 0) {
        cmd = "COD_MAJOR_MISCELLANEOUS";
    }
    ret = ret + cmd;
    ret = ret + " Minor Device:";
    switch (major) {
        case major_device_class_t::COD_MAJOR_AUDIO_VIDEO:
            cmd = ((magic_enum::enum_flags_name((av_minor_class_t)minor)));
            if (minor == 0) {
                cmd = "COD_MINOR_AV_UNCATEGORIZED";
            }
            break;
        case major_device_class_t::COD_MAJOR_COMPUTER:
            cmd = ((magic_enum::enum_flags_name((computer_minor_class_t)minor)));
            if (minor == 0) {
                cmd = "COD_MINOR_COMPUTER_UNCATEGORIZED";
            }
            break;
        case major_device_class_t::COD_MAJOR_HEALTH:
            cmd = ((magic_enum::enum_flags_name((health_minor_class_t)minor)));

            if (minor == 0) {
                cmd = "COD_MINOR_HEALTH_UNDEFINED";
            }
            break;
        case major_device_class_t::COD_MAJOR_IMAGING:
            cmd = ((magic_enum::enum_flags_name((imaging_minor_class_t)minor)));
            if (minor == 0) {
                cmd = "COD_IMAGING_UNCATEGORIZED";
            }
            break;

        case major_device_class_t::COD_MAJOR_NETWORK:
            cmd = ((magic_enum::enum_flags_name((lan_minor_class_t)minor)));
            if (minor == 0) {
                cmd = "COD_MINOR_LAN_UTIL_FULLY_AVAILABLE";
            }
            break;

        case major_device_class_t::COD_MAJOR_PERIPHERAL:
            cmd = ((magic_enum::enum_flags_name((peripheral_pointing_device_t)minor)));
            if (minor == 0) {
                cmd = "COD_MINOR_PERIPHERA_UNCATEGORIZED";
            }
            break;
        case major_device_class_t::COD_MAJOR_PHONE:
            cmd = ((magic_enum::enum_flags_name((phone_minor_class_t)minor)));
            if (minor == 0) {
                cmd = "COD_MINOR_PHONE_UNCATEGORIZED";
            }
            break;
        case major_device_class_t::COD_MAJOR_TOY:
            cmd = ((magic_enum::enum_flags_name((toy_minor_class_t)minor)));
            if (minor == 0) {
                cmd = "COD_MINOR_TOY_UNCATEGORIZED";
            }
            break;

        case major_device_class_t::COD_MAJOR_WEARABLE:
            cmd = ((magic_enum::enum_flags_name((wearable_minor_class_t)minor)));
            if (minor == 0) {
                cmd = "COD_MINOR_WEARABLE_UNCATEGORIZED";
            }
            break;

        default:
            cmd = "Undefined";
            break;
    }
    ret = ret + cmd + " ";
#endif
    return ret;
}

std::string decodeClassOfDevice(service_class_t services, major_device_class_t major, uint8_t minor) {
    std::string ret = "MAGIC Enum not enabled";

#if defined(CONFIG_NIMBLE_USE_MAGIC_ENUM)
    ret             = "Service Class:";
    std::string cmd = "";
    cmd             = magic_enum::enum_flags_name(services);
    if (services == 0) {
        cmd = "COD_SERVICE_NA";
    }
    ret = ret + cmd;
    cmd = decodeMajorMinor(major, minor);
    ret = ret + cmd;
#endif

    return ret;
}

std::string decodeClassOfDevice(bluetooth_cod_t cod) {
    NIMBLE_LOGI(LOG_TAG,
                "In DeCODE COS - RAW HEX cod value only 3 bytes are used  %s",
                HEXBuilder::bytes2hex((((const unsigned char*)&cod)), 3).c_str());
    return decodeClassOfDevice(getH_LService(cod), (major_device_class_t)cod.bit_field.major_device, cod.bit_field.minor_device);
}

std::vector<uint8_t> makeATT_Payload_CodeClassOfDevice(bluetooth_cod_t cod) {
    uint8_t out[5];

    // we are folling little edinan format here
    out[0] = 4;                            // length
    out[1] = BLE_HS_ADV_TYPE_CLASS_OF_DEV; // class of device
    out[2] = (cod.bytes3.byte0);
    out[3] = (cod.bytes3.byte1);
    out[4] = (cod.bytes3.byte2);
    std::vector<uint8_t> v(out, out + 5);

    NIMBLE_LOGI(LOG_TAG, " COB Byte0:%02x Byte1:%02x Byte2:%02x", (cod.bytes3.byte0), (cod.bytes3.byte1), (cod.bytes3.byte2));
    NIMBLE_LOGI(LOG_TAG,
                " Convetred COB %s to bytes in hex for ADV playload %s",
                decodeClassOfDevice(cod).c_str(),
                HEXBuilder::bytes2hex(v.data(), v.size()).c_str());

    return v;
}

} // namespace NimClassOfDeviceType