#include "bench_protocol.hpp"
#include "bench_build.hpp"
#include <NimBLEDevice.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>
#include <cJSON.h>
#include <driver/usb_serial_jtag.h>
#include <driver/usb_serial_jtag_vfs.h>
#include <esp_app_desc.h>
#include <esp_console.h>
#include <esp_random.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <fcntl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <host/ble_gap.h>

namespace {
constexpr auto serviceUuid = "e7100001-61b3-4b3d-8b4d-7e3b51e2c001";
constexpr auto psmUuid = "e7100002-61b3-4b3d-8b4d-7e3b51e2c001";
constexpr uint16_t serverPsm = 192, channelMtu = 5000;
QueueHandle_t replies, reflectRequests;
NimBLEClient* client = nullptr;
NimBLEL2CAPChannel* ownedChannel = nullptr;
std::atomic<NimBLEL2CAPChannel*> activeChannel{nullptr};
std::atomic<bool> serverMode{false}, connected{false}, waiting{false};
std::atomic<bool> reflectorMode{false}, advertisingEnabled{false};
std::atomic<uint16_t> gapHandle{BLE_HS_CONN_HANDLE_NONE};
std::atomic<uint32_t> connectionGeneration{0};
bool serverStarted = false;
std::atomic<uint32_t> firstCallbackUs{0}, callbackFaults{0};
std::atomic<uint16_t> negotiatedMtu{0};
std::atomic<uint16_t> observedTxOctets{0}, observedRxOctets{0};
std::atomic<int> intervalStatus{0};
std::atomic<bool> reflectorBusy{false};
std::mutex streamMutex;
bench::StreamReceiver streamReceiver;
int64_t requestStartedUs = 0;
uint16_t intervalUnits = 12;
uint8_t wantedPhy = BLE_GAP_LE_PHY_2M;

void emit(const char* event, cJSON* object = nullptr) {
    if (!object) object = cJSON_CreateObject();
    cJSON_AddStringToObject(object, "event", event);
    char* line = cJSON_PrintUnformatted(object);
    if (line) {
        const std::string output = std::string("BENCH ") + line + "\n";
        // IDF's stdout VFS can silently discard characters after a USB TX
        // timeout. Benchmark evidence must be complete; these writes occur
        // outside the measured request loop and can wait for the host.
        size_t offset = 0;
        while (offset < output.size()) {
            const size_t size = std::min<size_t>(512, output.size() - offset);
            const int written = usb_serial_jtag_write_bytes(output.data() + offset, size, portMAX_DELAY);
            if (written > 0) offset += written;
        }
        cJSON_free(line);
    }
    cJSON_Delete(object);
}
void error(const char* message) {
    auto object = cJSON_CreateObject();
    cJSON_AddStringToObject(object, "message", message);
    emit("error", object);
}
void number(cJSON* object, const char* key, double value) { cJSON_AddNumberToObject(object, key, value); }
void clearQueue(QueueHandle_t queue) {
    bench::Frame* frame;
    while (xQueueReceive(queue, &frame, 0) == pdTRUE) delete frame;
}
void clearStream() { std::lock_guard<std::mutex> lock(streamMutex); streamReceiver = {}; }
void heapStatus(const char* phase) {
    auto object = cJSON_CreateObject();
    cJSON_AddStringToObject(object, "phase", phase);
    number(object, "free", esp_get_free_heap_size());
    number(object, "minimum_ever", esp_get_minimum_free_heap_size());
    number(object, "largest_internal_block", heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    number(object, "internal_free", heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    number(object, "tasks", uxTaskGetNumberOfTasks()); emit("heap", object);
}

class ChannelCallbacks : public NimBLEL2CAPChannelCallbacks {
    bench::Parser parser;
public:
    void onConnect(NimBLEL2CAPChannel* channel, uint16_t mtu) override {
        ++connectionGeneration;
        clearQueue(replies); clearQueue(reflectRequests);
        clearStream();
        parser.reset(); negotiatedMtu = mtu; activeChannel = channel; connected = true;
    }
    void onRead(NimBLEL2CAPChannel*, std::vector<uint8_t>& bytes) override {
        const auto now = esp_timer_get_time();
        if (waiting.load()) {
            uint32_t empty = 0;
            firstCallbackUs.compare_exchange_strong(empty, uint32_t(now - requestStartedUs));
        }
        if (!parser.feed(bytes, [now](bench::Frame frame) {
            frame.receivedUs = now;
            frame.generation = connectionGeneration.load();
            if (frame.type == bench::stream) {
                std::lock_guard<std::mutex> lock(streamMutex);
                if (!streamReceiver.receive(frame.payload, now)) ++callbackFaults;
                return;
            }
            auto pending = new bench::Frame(std::move(frame));
            auto queue = reflectorMode.load() ? reflectRequests : replies;
            if (xQueueSend(queue, &pending, 0) != pdTRUE) { delete pending; ++callbackFaults; }
        })) ++callbackFaults;
    }
    void onDisconnect(NimBLEL2CAPChannel*) override {
        connected = false; activeChannel = nullptr;
        ++connectionGeneration;
    }
};

class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* peer) override { gapHandle = peer->getConnHandle(); peer->setDataLen(251); }
    void onDisconnect(NimBLEClient*, int) override { gapHandle = BLE_HS_CONN_HANDLE_NONE; }
} clientCallbacks;

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* server, NimBLEConnInfo& info) override {
        gapHandle = info.getConnHandle();
        server->setDataLen(info.getConnHandle(), 251);
        server->updateConnParams(info.getConnHandle(), intervalUnits, intervalUnits, 0, 200);
    }
    void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
        gapHandle = BLE_HS_CONN_HANDLE_NONE;
        if (advertisingEnabled.load()) NimBLEDevice::startAdvertising();
    }
};

void metadata() {
    auto object = cJSON_CreateObject();
    cJSON_AddStringToObject(object, "target", CONFIG_IDF_TARGET);
    number(object, "bench_protocol", bench::version);
    cJSON_AddStringToObject(object, "gap_role", serverMode.load() ? "peripheral" : "central");
    cJSON_AddStringToObject(object, "app_role", reflectorMode.load() ? "reflector" : "dut");
    cJSON_AddStringToObject(object, "library_commit", BENCH_LIBRARY_COMMIT);
    cJSON_AddStringToObject(object, "idf_commit", BENCH_IDF_COMMIT);
    cJSON_AddStringToObject(object, "sdkconfig_sha256", BENCH_CONFIG_HASH);
    const auto app = esp_app_get_description();
    char hash[65];
    for (size_t i = 0; i < 32; ++i) snprintf(hash + 2 * i, 3, "%02x", app->app_elf_sha256[i]);
    cJSON_AddStringToObject(object, "elf_sha256", hash);
    cJSON_AddStringToObject(object, "ble_address", NimBLEDevice::getAddress().toString().c_str());
    number(object, "cpu_mhz", CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);
    number(object, "msys1_block_size", CONFIG_BT_NIMBLE_MSYS_1_BLOCK_SIZE);
    number(object, "msys1_block_count", CONFIG_BT_NIMBLE_MSYS_1_BLOCK_COUNT);
    number(object, "msys2_block_size", CONFIG_BT_NIMBLE_MSYS_2_BLOCK_SIZE);
    number(object, "msys2_block_count", CONFIG_BT_NIMBLE_MSYS_2_BLOCK_COUNT);
    number(object, "acl_buf_count", CONFIG_BT_NIMBLE_ACL_BUF_COUNT);
    number(object, "sdu_buffers", CONFIG_NIMBLE_CPP_L2CAP_SDU_BUFFER_COUNT);
#ifdef CONFIG_NIMBLE_CPP_L2CAP_DEFERRED_READ_CALLBACKS
    cJSON_AddBoolToObject(object, "deferred_callbacks", true);
#else
    cJSON_AddBoolToObject(object, "deferred_callbacks", false);
#endif
#if defined(CONFIG_COMPILER_OPTIMIZATION_PERF)
    cJSON_AddStringToObject(object, "optimization", "performance");
#elif defined(CONFIG_COMPILER_OPTIMIZATION_SIZE)
    cJSON_AddStringToObject(object, "optimization", "size");
#else
    cJSON_AddStringToObject(object, "optimization", "debug");
#endif
    emit("hello", object);
}

void params() {
    auto object = cJSON_CreateObject();
    cJSON_AddBoolToObject(object, "connected", connected.load());
    cJSON_AddStringToObject(object, "gap_role", serverMode.load() ? "peripheral" : "central");
    number(object, "mtu", negotiatedMtu.load());
    number(object, "requested_interval_ms", intervalUnits * 1.25);
    number(object, "requested_phy", wantedPhy);
    number(object, "requested_tx_octets", 251);
    if (observedTxOctets.load()) {
        number(object, "tx_octets", observedTxOctets.load());
        number(object, "rx_octets", observedRxOctets.load());
    }
    ble_gap_conn_desc info{};
    const auto handle = gapHandle.load();
    if (handle != BLE_HS_CONN_HANDLE_NONE && ble_gap_conn_find(handle, &info) == 0) {
        number(object, "interval_ms", info.conn_itvl * 1.25);
        number(object, "latency", info.conn_latency);
        number(object, "supervision_ms", info.supervision_timeout * 10);
        int8_t rssi = 0;
        if (ble_gap_conn_rssi(handle, &rssi) == 0) number(object, "rssi_dbm", rssi);
        uint8_t txPhy = 0, rxPhy = 0;
        if (ble_gap_read_le_phy(handle, &txPhy, &rxPhy) == 0) {
            number(object, "tx_phy", txPhy); number(object, "rx_phy", rxPhy);
        }
    }
    emit("params", object);
}

bool disconnectClient() {
    waiting = false;
    advertisingEnabled = false;
    if (serverStarted) NimBLEDevice::stopAdvertising();
    auto channel = activeChannel.load();
    if (!channel) channel = ownedChannel;
    if (channel) channel->disconnect();
    if (serverMode.load() && gapHandle.load() != BLE_HS_CONN_HANDLE_NONE)
        NimBLEDevice::getServer()->disconnect(gapHandle.load());
    if (client && client->isConnected()) client->disconnect();
    const auto deadline = esp_timer_get_time() + 3000000;
    while (((channel && !channel->isIdle()) || gapHandle.load() != BLE_HS_CONN_HANDLE_NONE || reflectorBusy.load()) &&
           esp_timer_get_time() < deadline) vTaskDelay(1);
    if ((channel && !channel->isIdle()) || gapHandle.load() != BLE_HS_CONN_HANDLE_NONE || reflectorBusy.load()) {
        error("channel teardown still busy"); return false;
    }
    delete ownedChannel; ownedChannel = nullptr;
    if (client) {
        NimBLEDevice::deleteClient(client); client = nullptr;
        // Scan results include unrelated nearby advertisers. Keeping them makes
        // post-teardown heap depend on the last scan rather than channel leaks.
        NimBLEDevice::getScan()->clearResults();
    }
    activeChannel = nullptr; connected = false;
    negotiatedMtu = 0; observedTxOctets = 0; observedRxOctets = 0;
    intervalStatus = 0;
    clearQueue(replies); clearQueue(reflectRequests); clearStream();
    return true;
}

std::unique_ptr<bench::Frame> exchange(uint8_t type, const std::vector<uint8_t>& payload,
                                       uint8_t expected, uint32_t timeoutMs = 5000) {
    auto channel = activeChannel.load();
    if (!channel || !connected.load()) { error("no L2CAP connection"); return {}; }
    if (!channel->write(bench::encode(type, payload))) { error("L2CAP write failed"); return {}; }
    bench::Frame* frame = nullptr;
    if (xQueueReceive(replies, &frame, pdMS_TO_TICKS(timeoutMs)) != pdTRUE) {
        error("response timeout"); return {};
    }
    std::unique_ptr<bench::Frame> result(frame);
    if (frame->type != expected) { error("unexpected response type"); return {}; }
    return result;
}

bool greetPeer(uint16_t psm) {
    for (int retry = 0; retry < 3; ++retry) {
        if (ble_gap_set_prefered_le_phy(gapHandle.load(), wantedPhy, wantedPhy, 0) == 0) break;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    // Setup only: the product firmware retries transient interval collisions
    // too. Never wait or retry on a GAP callback, and never assume acceptance.
    for (int attempt = 1; attempt <= 3; ++attempt) {
        ble_gap_conn_desc info{};
        const auto handle = gapHandle.load();
        if (ble_gap_conn_find(handle, &info) != 0 ||
            (info.conn_itvl == intervalUnits && info.conn_latency == 0)) break;
        vTaskDelay(pdMS_TO_TICKS(1000));
        ble_gap_upd_params update{};
        update.itvl_min = update.itvl_max = intervalUnits;
        update.latency = 0; update.supervision_timeout = 200;
        const int priorStatus = intervalStatus.load();
        intervalStatus = -1;
        const int rc = ble_gap_update_params(handle, &update);
        const auto deadline = esp_timer_get_time() + 1000000;
        if (rc == 0 || rc == BLE_HS_EALREADY || rc == BLE_HS_EBUSY) {
            while (esp_timer_get_time() < deadline) {
                if (ble_gap_conn_find(handle, &info) != 0 || info.conn_itvl == intervalUnits) break;
                if (intervalStatus.load() != -1) break;
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }
        auto object = cJSON_CreateObject();
        number(object, "attempt", attempt); number(object, "request_rc", rc);
        number(object, "prior_status", priorStatus);
        number(object, "completion_status", intervalStatus.load());
        number(object, "actual_interval_ms", info.conn_itvl * 1.25); emit("interval-update", object);
        const int status = rc != 0 ? rc : intervalStatus.load();
        const bool transient = status == -1 || status == BLE_HS_EBUSY || status == BLE_HS_EALREADY ||
            status == BLE_HS_HCI_ERR(BLE_ERR_DIFF_TRANS_COLL) || status == BLE_HS_HCI_ERR(BLE_ERR_LMP_COLLISION);
        if (!transient) break;
    }
    std::string identity = std::string(CONFIG_IDF_TARGET) + " " + BENCH_LIBRARY_COMMIT;
    std::vector<uint8_t> hello{bench::version}; hello.insert(hello.end(), identity.begin(), identity.end());
    auto answer = exchange(bench::hello, hello, bench::helloReply);
    if (!answer || answer->payload.empty() || answer->payload[0] != bench::version) {
        error("HELLO failed"); disconnectClient(); return false;
    }
    auto object = cJSON_CreateObject();
    const std::string peer(answer->payload.begin() + 1, answer->payload.end());
    cJSON_AddStringToObject(object, "peer_identity", peer.c_str());
    cJSON_AddStringToObject(object, "gap_role", serverMode.load() ? "peripheral" : "central");
    number(object, "psm", psm); emit("connected", object); params(); return true;
}

bool connectPeer(const char* name) {
    if (!disconnectClient()) return false;
    serverMode = false; reflectorMode = false;
    auto scan = NimBLEDevice::getScan();
    scan->setActiveScan(true); scan->clearResults();
    const auto results = scan->getResults(5000);
    const NimBLEAdvertisedDevice* found = nullptr;
    size_t matches = 0;
    for (auto peer : results) {
        const bool serviceMatch = peer->isAdvertisingService(NimBLEUUID(serviceUuid));
        const bool identityMatch = (peer->haveName() && peer->getName() == name) || peer->getAddress().toString() == name;
        const bool selected = !strcmp(name, "auto") ? serviceMatch : identityMatch;
        if (!serviceMatch && !identityMatch) continue;
        auto object = cJSON_CreateObject();
        cJSON_AddStringToObject(object, "name", peer->getName().c_str());
        cJSON_AddStringToObject(object, "address", peer->getAddress().toString().c_str());
        cJSON_AddBoolToObject(object, "bench_service", serviceMatch);
        number(object, "rssi_dbm", peer->getRSSI()); emit("discovered", object);
        if (selected) { found = peer; ++matches; }
    }
    if (matches > 1) { error("multiple benchmark reflectors; select a name or BLE address"); return false; }
    if (!found) { error("named reflector not found"); return false; }
    client = NimBLEDevice::createClient();
    client->setClientCallbacks(&clientCallbacks, false);
    client->setConnectionParams(intervalUnits, intervalUnits, 0, 200);
    client->setConnectTimeout(10000);
    if (!client->connect(found)) { error("GAP connect failed"); disconnectClient(); return false; }
    auto service = client->getService(serviceUuid);
    auto characteristic = service ? service->getCharacteristic(psmUuid) : nullptr;
    const auto value = characteristic ? characteristic->readValue() : NimBLEAttValue();
    if (value.size() != 2) { error("PSM discovery failed"); disconnectClient(); return false; }
    const auto psm = uint16_t(value[0]) | (uint16_t(value[1]) << 8);
    ownedChannel = NimBLEL2CAPChannel::connect(client, psm, channelMtu, new ChannelCallbacks());
    if (!ownedChannel) { error("L2CAP connect failed"); disconnectClient(); return false; }
    return greetPeer(psm);
}

#include "bench_stream_run.inc"

struct Sample { uint32_t sequence, rtt, first, submit, turnaround; };
int run(int argc, char** argv) {
    if (argc != 5) { error("usage: run echo|request|stream-tx|stream-rx|duplex request_size response_size count"); return 1; }
    const long req = strtol(argv[2], nullptr, 10), resp = strtol(argv[3], nullptr, 10), count = strtol(argv[4], nullptr, 10);
    const uint8_t pattern = !strcmp(argv[1], "echo") ? 1 : !strcmp(argv[1], "request") ? 2 :
        !strcmp(argv[1], "stream-tx") ? 3 : !strcmp(argv[1], "stream-rx") ? 4 : !strcmp(argv[1], "duplex") ? 5 : 0;
    if (!pattern || req < 8 || req > bench::maxPayload || resp < 12 || resp > bench::maxPayload ||
        count < 1 || count > 4096 || (pattern == 1 && req != resp)) {
        error("invalid pattern, sizes or count (1..4096)"); return 1;
    }
    if (!connected.load() || reflectorMode.load()) { error("connect to a reflector first"); return 1; }
    params();
    bench::Config config{pattern, uint16_t(req), uint16_t(resp), uint32_t(count)};
    auto ack = exchange(bench::config, config.encode(), bench::configReply);
    if (!ack || ack->payload != std::vector<uint8_t>{0}) { disconnectClient(); return 1; }
    if (pattern >= 3) return runStream(config, argv[1]);
    auto startObject = cJSON_CreateObject();
    cJSON_AddStringToObject(startObject, "pattern", argv[1]);
    number(startObject, "request_size", req); number(startObject, "response_size", resp); number(startObject, "count", count);
    number(startObject, "heap_free", esp_get_free_heap_size()); emit("run-start", startObject);
    std::vector<Sample> samples; samples.reserve(count);
    const uint32_t token = esp_random();
    const auto faultsBefore = callbackFaults.load();
    const auto started = esp_timer_get_time();
    bool success = true;
    for (uint32_t seq = 0; seq < uint32_t(count); ++seq) {
        std::vector<uint8_t> payload; payload.reserve(req);
        bench::put32(payload, seq); bench::put32(payload, token);
        for (size_t i = 0; i < size_t(req - 8); ++i) payload.push_back(bench::pattern(seq, token, i));
        firstCallbackUs = 0; requestStartedUs = esp_timer_get_time(); waiting = true;
        auto channel = activeChannel.load();
        const bool written = channel && channel->write(bench::encode(bench::ping, payload));
        const auto submitUs = uint32_t(esp_timer_get_time() - requestStartedUs);
        bench::Frame* raw = nullptr;
        const bool received = written && xQueueReceive(replies, &raw, pdMS_TO_TICKS(5000)) == pdTRUE;
        waiting = false;
        std::unique_ptr<bench::Frame> reply(raw);
        if (!received) { error(written ? "PING timeout" : "PING write failed"); success = false; break; }
        const auto& p = reply->payload;
        bool valid = reply->type == bench::pingReply && p.size() == size_t(resp) &&
                     bench::u32(p.data()) == seq && bench::u32(p.data() + 4) == token;
        for (size_t i = 12; valid && i < p.size(); ++i) valid = p[i] == bench::pattern(seq, token, i - 12);
        const auto rtt = uint32_t(reply->receivedUs - requestStartedUs);
        const auto turnaround = valid ? bench::u32(p.data() + 8) : 0;
        if (!valid || turnaround > rtt) { error("PING length, sequence, pattern or timing mismatch"); success = false; break; }
        samples.push_back({seq, rtt, firstCallbackUs.load(), submitUs, turnaround});
    }
    const auto elapsed = esp_timer_get_time() - started;
    params();
    success = success && callbackFaults.load() == faultsBefore;
    std::vector<uint32_t> rtts; uint64_t sum = 0;
    for (const auto& sample : samples) { rtts.push_back(sample.rtt); sum += sample.rtt; }
    std::sort(rtts.begin(), rtts.end());
    auto result = cJSON_CreateObject();
    cJSON_AddBoolToObject(result, "success", success);
    number(result, "completed", samples.size()); number(result, "elapsed_us", elapsed);
    number(result, "callback_faults", callbackFaults.load() - faultsBefore);
    if (!rtts.empty()) {
        number(result, "rtt_min_us", rtts.front()); number(result, "rtt_max_us", rtts.back());
        number(result, "rtt_median_us", (double(rtts[(rtts.size() - 1) / 2]) + rtts[rtts.size() / 2]) / 2);
        number(result, "rtt_p95_us", rtts[(rtts.size() * 95 + 99) / 100 - 1]);
        number(result, "rtt_mean_us", double(sum) / samples.size());
        number(result, "goodput_bytes_per_s", double(samples.size()) * (req + resp - 20) * 1e6 / elapsed);
        number(result, "framed_bytes_per_s", double(samples.size()) * (req + resp + 8) * 1e6 / elapsed);
    }
    number(result, "heap_free", esp_get_free_heap_size()); number(result, "heap_min", esp_get_minimum_free_heap_size());
    emit("run-result", result);
    // Serialize samples after timing stops, so the serial link cannot pace the test.
    for (const auto& sample : samples) {
        auto object = cJSON_CreateObject();
        number(object, "seq", sample.sequence); number(object, "rtt_us", sample.rtt);
        number(object, "first_callback_us", sample.first); number(object, "tx_submit_us", sample.submit);
        number(object, "peer_turnaround_us", sample.turnaround);
        number(object, "adjusted_rtt_us", sample.rtt - sample.turnaround); emit("sample", object);
    }
    emit("run-end"); if (!success) disconnectClient(); return success ? 0 : 1;
}

void reflectorTask(void*) {
    bench::Config config;
    uint32_t expectedSequence = 0;
    uint32_t generation = 0, streamToken = 0;
    bool streaming = false, rxEnded = false, txConfirmed = false;
    for (;;) {
        bench::Frame* raw;
        if (xQueueReceive(reflectRequests, &raw, portMAX_DELAY) != pdTRUE) continue;
        std::unique_ptr<bench::Frame> request(raw);
        struct Busy { Busy() { reflectorBusy = true; } ~Busy() { reflectorBusy = false; } } busy;
        auto channel = activeChannel.load();
        if (!channel || !connected.load() || request->generation != connectionGeneration.load()) continue;
        if (generation != request->generation) { generation = request->generation; config = {}; streaming = false; }
        uint8_t type = bench::abort;
        std::vector<uint8_t> response;
        if (request->type == bench::hello && !request->payload.empty() && request->payload[0] == bench::version) {
            config = {}; streaming = false;
            const std::string identity = std::string(CONFIG_IDF_TARGET) + " " + BENCH_LIBRARY_COMMIT;
            response = {bench::version}; response.insert(response.end(), identity.begin(), identity.end()); type = bench::helloReply;
        } else if (request->type == bench::config && !streaming && config.parse(request->payload)) {
            expectedSequence = 0;
            response = {0}; type = bench::configReply;
        } else if (request->type == bench::streamStart && config.pattern >= 3 && !streaming && request->payload.size() == 4) {
            streamToken = bench::u32(request->payload.data()); streaming = true;
            rxEnded = config.pattern == 4; txConfirmed = config.pattern == 3;
            { std::lock_guard<std::mutex> lock(streamMutex);
              streamReceiver.reset(streamToken, config.pattern != 4 ? config.count : 0, config.request); }
            bool ok = channel->write(bench::encode(bench::streamStartReply, request->payload));
            if (ok && config.pattern != 3) ok = sendStream(channel, generation, streamToken, config.count, config.response);
            if (!ok) { ++callbackFaults; channel->disconnect(); }
            continue;
        } else if (request->type == bench::streamEnd && streaming && !rxEnded) {
            std::lock_guard<std::mutex> lock(streamMutex);
            if (streamReceiver.end(request->payload)) {
                rxEnded = true; type = bench::streamEndReply; response = streamReceiver.summary.encode();
            }
        } else if (request->type == bench::streamEndReply && streaming && !txConfirmed) {
            bench::StreamSummary summary;
            if (!summary.parse(request->payload) || !summary.matches(streamToken, config.count, config.response)) {
                ++callbackFaults; channel->disconnect();
            } else txConfirmed = true;
            continue;
        } else if (request->type == bench::streamFinish && streaming && rxEnded && txConfirmed &&
                   request->payload.size() == 4 && bench::u32(request->payload.data()) == streamToken) {
            streaming = false; clearStream(); type = bench::streamFinishReply; response = request->payload;
        } else if (request->type == bench::ping && request->payload.size() == config.request && config.request >= 8) {
            const auto seq = bench::u32(request->payload.data()), token = bench::u32(request->payload.data() + 4);
            bool valid = seq == expectedSequence && seq < config.count;
            for (size_t i = 8; i < request->payload.size(); ++i)
                if (request->payload[i] != bench::pattern(seq, token, i - 8)) valid = false;
            if (valid) {
                bench::put32(response, seq); bench::put32(response, token);
                ++expectedSequence;
                bench::put32(response, 0);
                for (size_t i = 0; i < size_t(config.response - 12); ++i) response.push_back(bench::pattern(seq, token, i));
                type = bench::pingReply;
            }
        }
        auto encoded = bench::encode(type, response);
        if (type == bench::pingReply) {
            const auto turnaround = uint32_t(esp_timer_get_time() - request->receivedUs);
            for (int i = 0; i < 4; ++i) encoded[12 + i] = uint8_t(turnaround >> ((3 - i) * 8));
        }
        if (generation == connectionGeneration.load() && !channel->write(encoded)) ++callbackFaults;
    }
}

int command(int argc, char** argv) {
    if (!strcmp(argv[0], "run")) {
        const int result = run(argc, argv);
        heapStatus("after-run");
        return result;
    }
    if (!strcmp(argv[0], "connect")) return argc == 2 && connectPeer(argv[1]) ? 0 : 1;
    if (!strcmp(argv[0], "disconnect")) {
        const bool ok = disconnectClient();
        vTaskDelay(pdMS_TO_TICKS(100)); // Let FreeRTOS reclaim deleted tasks before heap sampling.
        emit("disconnected"); heapStatus("disconnected"); return ok ? 0 : 1;
    }
    if (!strcmp(argv[0], "status")) { metadata(); params(); heapStatus(connected.load() ? "connected" : "idle"); return 0; }
    if (!strcmp(argv[0], "reboot")) esp_restart();
    if (!strcmp(argv[0], "params")) {
        if (argc == 3) {
            const double interval = strtod(argv[1], nullptr);
            const int phy = atoi(argv[2]);
            if (connected.load() || interval < 7.5 || interval > 4000 || (phy != 1 && phy != 2)) {
                error("disconnect first; params <interval_ms> <1|2>"); return 1;
            }
            intervalUnits = uint16_t(interval / 1.25); wantedPhy = phy == 2 ? BLE_GAP_LE_PHY_2M : BLE_GAP_LE_PHY_1M;
        }
        params(); return 0;
    }
    if (!strcmp(argv[0], "role") && argc == 2 && !strcmp(argv[1], "client")) {
        if (!disconnectClient()) return 1;
        serverMode = false; reflectorMode = false; emit("client-ready"); return 0;
    }
    if (!strcmp(argv[0], "await") && argc == 2) {
        const long seconds = strtol(argv[1], nullptr, 10);
        if (seconds < 1 || seconds > 120 || reflectorMode.load()) return 1;
        const auto deadline = esp_timer_get_time() + seconds * 1000000LL;
        while (!connected.load() && esp_timer_get_time() < deadline) vTaskDelay(pdMS_TO_TICKS(10));
        if (!connected.load()) { error("waiting for peer timed out"); return 1; }
        return greetPeer(serverPsm) ? 0 : 1;
    }
    if (!strcmp(argv[0], "listen") ||
        (!strcmp(argv[0], "role") && argc == 2 && !strcmp(argv[1], "server"))) {
        if (serverMode.load() && connected.load()) { error("disconnect before changing role"); return 1; }
        if (!disconnectClient()) return 1;
        serverMode = true;
        reflectorMode = strcmp(argv[0], "listen") != 0;
        if (!serverStarted) {
            auto coc = NimBLEDevice::createL2CAPServer();
            if (!coc->createService(serverPsm, channelMtu, new ChannelCallbacks())) { error("server create failed"); return 1; }
            auto server = NimBLEDevice::createServer(); server->setCallbacks(new ServerCallbacks());
            auto service = server->createService(serviceUuid);
            auto characteristic = service->createCharacteristic(psmUuid, NIMBLE_PROPERTY::READ);
            const uint8_t psm[] = {uint8_t(serverPsm), uint8_t(serverPsm >> 8)};
            characteristic->setValue(psm, sizeof(psm));
            auto advertising = NimBLEDevice::getAdvertising();
            NimBLEAdvertisementData scanResponse; scanResponse.setName("L2CAP-Bench-ESP");
            advertising->setScanResponseData(scanResponse); advertising->addServiceUUID(serviceUuid);
            serverStarted = true;
        }
        auto advertising = NimBLEDevice::getAdvertising();
        advertisingEnabled = true;
        if (!advertising->start()) { error("advertising failed"); return 1; }
        emit(reflectorMode.load() ? "server-ready" : "listening"); return 0;
    }
    error("unknown command; role server|client, listen, await <seconds>, connect <peer>, run echo|request <req> <resp> <count>, params <ms> <1|2>, status, disconnect, reboot");
    return 1;
}
} // namespace

extern "C" void app_main() {
    setvbuf(stdin, nullptr, _IONBF, 0); setvbuf(stdout, nullptr, _IONBF, 0);
    usb_serial_jtag_driver_config_t usbConfig{}; usbConfig.rx_buffer_size = 1024; usbConfig.tx_buffer_size = 4096;
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usbConfig));
    usb_serial_jtag_vfs_set_rx_line_endings(ESP_LINE_ENDINGS_LF);
    usb_serial_jtag_vfs_set_tx_line_endings(ESP_LINE_ENDINGS_LF);
    usb_serial_jtag_vfs_use_driver(); fcntl(fileno(stdin), F_SETFL, 0);
    replies = xQueueCreate(16, sizeof(bench::Frame*)); reflectRequests = xQueueCreate(16, sizeof(bench::Frame*));
    configASSERT(replies && reflectRequests);
    NimBLEDevice::init("L2CAP-Bench-ESP"); NimBLEDevice::setMTU(517);
    NimBLEDevice::setCustomGapHandler([](ble_gap_event* event, void*) -> int {
        if (event->type == BLE_GAP_EVENT_DATA_LEN_CHG) {
            observedTxOctets = event->data_len_chg.max_tx_octets;
            observedRxOctets = event->data_len_chg.max_rx_octets;
        } else if (event->type == BLE_GAP_EVENT_CONN_UPDATE) {
            intervalStatus = event->conn_update.status;
        }
        return 0;
    });
    xTaskCreate(reflectorTask, "reflect", 6144, nullptr, 4, nullptr);
    esp_console_config_t console{}; console.max_cmdline_args = 8; console.max_cmdline_length = 256;
    ESP_ERROR_CHECK(esp_console_init(&console));
    for (auto name : {"connect", "disconnect", "run", "status", "params", "role", "listen", "await", "reboot"}) {
        esp_console_cmd_t cmd{}; cmd.command = name; cmd.help = "L2CAP benchmark command"; cmd.func = command;
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
    }
    metadata(); emit("ready");
    char line[256];
    while (true) {
        if (!fgets(line, sizeof(line), stdin)) { clearerr(stdin); vTaskDelay(1); continue; }
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0]) continue;
        int result = 0;
        const auto err = esp_console_run(line, &result);
        if (err != ESP_OK) error(esp_err_to_name(err));
        auto object = cJSON_CreateObject(); number(object, "code", err == ESP_OK ? result : int(err)); emit("command-end", object);
    }
}
