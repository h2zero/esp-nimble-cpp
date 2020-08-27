#include "NimBLEDevice.h"
#include "driver/gpio.h"

extern "C" {void app_main(void);}

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"

// LED pins
#define LEDR GPIO_NUM_16
#define LEDG GPIO_NUM_17
#define LEDB GPIO_NUM_18
#define OUTPUT_PIN (1ULL<<LEDG)

static uint8_t onOffVal = 0; 
static int16_t levelVal = 0; 

class onOffSrvModelCallbacks : public NimBLEMeshModelCallbacks {
    void setOnOff(uint8_t val) {
        printf("on/off set val %d\n", val);
        onOffVal = val;
        gpio_set_level(LEDG, !onOffVal);
    }

    uint8_t getOnOff() {
        printf("on/off get val %d", onOffVal);
        return onOffVal;
    }
};

class levelSrvModelCallbacks : public NimBLEMeshModelCallbacks {
    void setLevel(int16_t val) {
        printf("Level set val %d\n", val);
        levelVal = val;
    }

    void setDelta(int16_t val) {
        printf("Level set delta %d\n", val);
        levelVal += val;
    }

    int16_t getLevel() {
        printf("Level get val %d\n", levelVal);
        return levelVal;
    }
};


void app_main(void) {
    gpio_config_t io_conf;
    io_conf.intr_type = (gpio_int_type_t)GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = OUTPUT_PIN;
    io_conf.pull_down_en = (gpio_pulldown_t)0;
    io_conf.pull_up_en = (gpio_pullup_t)0;
    gpio_config(&io_conf);
    
    gpio_set_level(LEDG, 1);
  
    NimBLEDevice::init("");
    NimBLEMeshNode *pMesh = NimBLEDevice::createMeshNode(NimBLEUUID(SERVICE_UUID),0);
    NimBLEMeshElement* pElem = pMesh->getElement();
    pElem->createModel(BT_MESH_MODEL_ID_GEN_ONOFF_SRV, new onOffSrvModelCallbacks());
    //pElem = pMesh->createElement();
    pElem->createModel(BT_MESH_MODEL_ID_GEN_LEVEL_SRV, new levelSrvModelCallbacks());
    pMesh->start();
}
