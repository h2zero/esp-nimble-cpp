//
// (C) Dr. Michael 'Mickey' Lauer <mickey@vanille-media.de>
//
#ifndef NIMBLEL2CAPSERVER_H
#define NIMBLEL2CAPSERVER_H

#include "inttypes.h"
#include <vector>

class NimBLEL2CAPService;
class NimBLEL2CAPServiceCallbacks;

#pragma once

class NimBLEL2CAPServer {
public:
    NimBLEL2CAPServer();
    ~NimBLEL2CAPServer();

    NimBLEL2CAPService* createService(const uint16_t psm, const uint16_t mtu, NimBLEL2CAPServiceCallbacks* callbacks);

private:
    friend class NimBLEL2CAPService;
    std::vector<NimBLEL2CAPService*> m_svcVec;
};

#endif