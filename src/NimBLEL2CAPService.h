//
// (C) Dr. Michael 'Mickey' Lauer <mickey@vanille-media.de>
//
#ifndef NIMBLEL2CAPSERVICE_H
#define NIMBLEL2CAPSERVICE_H

#pragma once

#include "inttypes.h"
#include "host/ble_l2cap.h"
#include "os/os_mbuf.h"
#include "os/os_mempool.h"
#undef max
#undef min

#include <vector>

#define L2CAP_BUF_BLOCK_SIZE            (250)
#define L2CAP_BUF_SIZE_MTUS_PER_CHANNEL (3)

class NimBLEL2CAPServiceCallbacks;

class NimBLEL2CAPService {
public:
    NimBLEL2CAPService(uint16_t psm, uint16_t mtu, NimBLEL2CAPServiceCallbacks* callbacks);
    ~NimBLEL2CAPService();

    void write(std::vector<uint8_t>& bytes);

protected:
    int handleConnectionEvent(struct ble_l2cap_event *event);
    int handleAcceptEvent(struct ble_l2cap_event *event);
    int handleDataReceivedEvent(struct ble_l2cap_event *event);
    int handleTxUnstalledEvent(struct ble_l2cap_event *event);
    int handleDisconnectionEvent(struct ble_l2cap_event *event);

private:
    uint16_t psm; // protocol service multiplexer
    uint16_t mtu; // maximum transmission unit
    struct ble_l2cap_chan* channel; // channel handle
    uint8_t* receiveBuffer; // MTU buffer

    NimBLEL2CAPServiceCallbacks* callbacks;

    void* _coc_memory;
    struct os_mempool _coc_mempool;
    struct os_mbuf_pool _coc_mbuf_pool;

    static int handleL2capEvent(struct ble_l2cap_event *event, void *arg);
};

class NimBLEL2CAPServiceCallbacks {

public:
    virtual      ~NimBLEL2CAPServiceCallbacks();
    virtual void onConnect(NimBLEL2CAPService* pService) = 0;
    virtual void onRead(NimBLEL2CAPService* pService, std::vector<uint8_t>& data) = 0;
    virtual void onDisconnect(NimBLEL2CAPService* pService) = 0;
};

#endif