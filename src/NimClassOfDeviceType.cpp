
#include <stdint.h>
#include <stdbool.h>
#include <WString.h>
#include "NimBLELog.h"
#include <HEXBuilder.h>
#include "magicEnum/magic_enum.hpp"
#include "magicEnum/magic_enum_iostream.hpp"
#include "NimClassOfDeviceType.h"

#define BLE_HS_ADV_TYPE_CLASS_OF_DEV 0x0d

static const char* LOG_TAG = "NimClassOfDeviceType";

namespace NimClassOfDeviceType {

// fucntion to add the serivce H aand L comps togetther
static NimClassOfDeviceType::service_class_t getH_LService(bluetooth_cod_t cod) {
    uint16_t service_highBit = (cod.bit_field.service_classH << 3);
    uint16_t service         = service_highBit | ((uint16_t)cod.bit_field.service_classL);
    NIMBLE_LOGE(LOG_TAG," Merging Service H (0x%02x) and L (0x%02x) into (0x%02x) ", service_highBit, cod.bit_field.service_classL, service);
    return static_cast<NimClassOfDeviceType::service_class_t>(service);
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
        NIMBLE_LOGE(LOG_TAG, "Breaking services 0x%02x into 3 bits 0x%02x and 8 bits 0x%02x", services, L_3Bit, H_8Bit);
    } else {
        // increasing bits from 11 to 16 as the esp32 core dumps at higher bits ( this filed (two bytes)is now across three bytes !)
        cod.bit_field.service_classL = services & 0x007; // only use the 3 lower bits
    }

    
    NIMBLE_LOGE(LOG_TAG,
                "Making servies %s, major %s, minor 0x%02x RAW HEX cod value only three bytes are used %s",
                ((magic_enum::enum_name(NimClassOfDeviceType::getH_LService(cod)))),
                ((magic_enum::enum_name((major_device_class_t)cod.bit_field.major_device))),
                minor,
                HEXBuilder::bytes2hex((((const unsigned char*)&cod)), 3).c_str());


    NIMBLE_LOGE(LOG_TAG,
                " Making COB Byte0:0x%02x Byte1:0x%02x Byte2:0x%02x",
                (cod.bytes3.byte0),
                (cod.bytes3.byte1),
                (cod.bytes3.byte2));
    return cod;
}

std::string decodeClassOfDevice(bluetooth_cod_t cod) {
    std::string ret = "Service Class:";
    std::string cmd = "";
    cmd             = (magic_enum::enum_name(NimClassOfDeviceType::getH_LService(cod)));
    ret             = ret + cmd;
    ret             = ret + " Major Device:";
    cmd             = ((magic_enum::enum_name((major_device_class_t)cod.bit_field.major_device)));
    ret             = ret + cmd;
    switch ((major_device_class_t)cod.bit_field.major_device) {
        case major_device_class_t::COD_MAJOR_AUDIO_VIDEO:
            cmd = ((magic_enum::enum_name((av_minor_class_t)cod.bit_field.minor_device)));
            break;
        case major_device_class_t::COD_MAJOR_COMPUTER:
            cmd = ((magic_enum::enum_name((computer_minor_class_t)cod.bit_field.minor_device)));
            break;
        case major_device_class_t::COD_MAJOR_HEALTH:
            cmd = ((magic_enum::enum_name((health_minor_class_t)cod.bit_field.minor_device)));
            break;
        case major_device_class_t::COD_MAJOR_IMAGING:
            cmd = ((magic_enum::enum_name((imaging_minor_class_t)cod.bit_field.minor_device)));
            break;

        case major_device_class_t::COD_MAJOR_NETWORK:
            cmd = ((magic_enum::enum_name((lan_minor_class_t)cod.bit_field.minor_device)));
            break;

        case major_device_class_t::COD_MAJOR_PERIPHERAL:
            cmd = ((magic_enum::enum_name((peripheral_pointing_device_t)cod.bit_field.minor_device)));
            break;
        case major_device_class_t::COD_MAJOR_PHONE:
            cmd = ((magic_enum::enum_name((phone_minor_class_t)cod.bit_field.minor_device)));
            break;
        case major_device_class_t::COD_MAJOR_TOY:
            cmd = ((magic_enum::enum_name((toy_minor_class_t)cod.bit_field.minor_device)));
            break;

        case major_device_class_t::COD_MAJOR_WEARABLE:
            cmd = ((magic_enum::enum_name((wearable_minor_class_t)cod.bit_field.minor_device)));
            break;

        default:
            cmd = "Undefined";
            break;
    }
    ret = ret + cmd;

    NIMBLE_LOGE(LOG_TAG,
                "In DeCODE COS - RAW HEX cod value only 3 bytes are used  %s",
                HEXBuilder::bytes2hex((((const unsigned char*)&cod)), 3).c_str());
    return ret;
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

    NIMBLE_LOGE(LOG_TAG, " COB Byte0:%02x Byte1:%02x Byte2:%02x", (cod.bytes3.byte0), (cod.bytes3.byte1), (cod.bytes3.byte2));
    NIMBLE_LOGE(LOG_TAG,
                " Convetred COB %s to bytes in hex for ADV playload %s",
                decodeClassOfDevice(cod).c_str(),
                HEXBuilder::bytes2hex(v.data(), v.size()).c_str());

    return v;
}

} // namespace NimClassOfDeviceType