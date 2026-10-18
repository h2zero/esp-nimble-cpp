#include "NimBLEDevice.h"

// Compile/link probe: exercise the server even when advertising is disabled.
extern "C" void app_main() {
    NimBLEDevice::init("server-build-probe");
    auto* server = NimBLEDevice::createServer();
    server->advertiseOnDisconnect(false);
    server->createService("180f");
}
