#include "NimBLEDevice.h"
#include "NimBLEL2CAPServer.h"
#include "NimBLEL2CAPChannel.h"
#include "freertos/queue.h"
#include <atomic>
#include <cassert>

// Compile/link probe and minimal worker-driven echo pattern. The service lives
// for the process lifetime; no callback deletes its channel or blocks for TX.
struct EchoJob {
    NimBLEL2CAPChannel* channel;
    uint32_t generation;
    std::vector<uint8_t> data;
};
static QueueHandle_t jobs;
static std::atomic<uint32_t> generation{0};

class EchoCallbacks : public NimBLEL2CAPChannelCallbacks {
    void onConnect(NimBLEL2CAPChannel*, uint16_t) override { ++generation; }
    void onDisconnect(NimBLEL2CAPChannel*) override { ++generation; }
    void onRead(NimBLEL2CAPChannel* channel, std::vector<uint8_t>& bytes) override {
        auto* job = new EchoJob{channel, generation.load(), bytes};
        if (xQueueSend(jobs, &job, 0) != pdTRUE) {
            delete job;
            channel->disconnect();
        }
    }
};

static void echoWorker(void*) {
    EchoJob* job;
    while (true) {
        if (xQueueReceive(jobs, &job, portMAX_DELAY) != pdTRUE) continue;
        if (job->generation == generation.load() && job->channel->isConnected()) {
            job->channel->write(job->data);
        }
        delete job;
    }
}

extern "C" void app_main() {
    jobs = xQueueCreate(4, sizeof(EchoJob*));
    assert(jobs);
    const auto created = xTaskCreate(echoWorker, "coc-echo", 4096, nullptr, 5, nullptr);
    assert(created == pdPASS);
    NimBLEDevice::init("coc-build-probe");
    auto* channel = NimBLEDevice::createL2CAPServer()->createService(129, 4096, new EchoCallbacks);
    (void)channel->isIdle();
}
