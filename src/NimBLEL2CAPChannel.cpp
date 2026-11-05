//
// (C) Dr. Michael 'Mickey' Lauer <mickey@vanille-media.de>
//

#include "NimBLEL2CAPChannel.h"
#if CONFIG_BT_NIMBLE_ENABLED && MYNEWT_VAL(BLE_L2CAP_COC_MAX_NUM)

# include "NimBLEClient.h"
# include "NimBLELog.h"
# include "NimBLEUtils.h"
# include "freertos/queue.h"
# include "freertos/task.h"
# ifdef USING_NIMBLE_ARDUINO_HEADERS
#  include "nimble/porting/nimble/include/nimble/nimble_port.h"
# else
#  include "nimble/nimble_port.h"
# endif
# include <algorithm>

# ifdef USING_NIMBLE_ARDUINO_HEADERS
#  include "nimble/nimble/host/include/host/ble_gap.h"
# else
#  include "host/ble_gap.h"
# endif

// Allocate one full SDU per mbuf, matching NimBLE's own CoC examples.
# define L2CAP_SDU_BUFFER_COUNT CONFIG_NIMBLE_CPP_L2CAP_SDU_BUFFER_COUNT
// Retry
constexpr uint32_t SendTimeoutMs = 2000;
#ifndef CONFIG_NIMBLE_CPP_L2CAP_CALLBACK_QUEUE_LENGTH
#define CONFIG_NIMBLE_CPP_L2CAP_CALLBACK_QUEUE_LENGTH 16
#define CONFIG_NIMBLE_CPP_L2CAP_CALLBACK_TASK_STACK_SIZE 4096
#define CONFIG_NIMBLE_CPP_L2CAP_CALLBACK_TASK_PRIORITY 5
#endif

static void logPoolSanityWarning(const char* tag, uint16_t psm, const ble_l2cap_chan_info& info) {
    const uint16_t negotiatedCocMtu = info.peer_coc_mtu < info.our_coc_mtu ? info.peer_coc_mtu : info.our_coc_mtu;
    const uint16_t effectiveL2capMtu = info.peer_l2cap_mtu < info.our_l2cap_mtu ? info.peer_l2cap_mtu : info.our_l2cap_mtu;

    if (negotiatedCocMtu == 0 || effectiveL2capMtu == 0) {
        return;
    }

    const uint32_t fragmentsPerSdu = (negotiatedCocMtu + effectiveL2capMtu - 1) / effectiveL2capMtu;
    const uint32_t msys1BlockCount = MYNEWT_VAL(MSYS_1_BLOCK_COUNT);
    const uint32_t msys1BlockSize = MYNEWT_VAL(MSYS_1_BLOCK_SIZE);
    const uint32_t cocMps = MYNEWT_VAL(BLE_L2CAP_COC_MPS);
    const uint32_t aclFromLlCount = MYNEWT_VAL(BLE_TRANSPORT_ACL_FROM_LL_COUNT);
    const uint32_t localSduBufferCount = L2CAP_SDU_BUFFER_COUNT;

    NIMBLE_LOGI(tag,
                "L2CAP COC 0x%04X path geometry: negotiated_coc=%u effective_l2cap=%u fragments_per_sdu=%lu mps=%lu local_sdu_bufs=%lu msys1=%lux%lu acl_from_ll=%lu",
                psm,
                negotiatedCocMtu,
                effectiveL2capMtu,
                (unsigned long)fragmentsPerSdu,
                (unsigned long)cocMps,
                (unsigned long)localSduBufferCount,
                (unsigned long)msys1BlockCount,
                (unsigned long)msys1BlockSize,
                (unsigned long)aclFromLlCount);

    if (effectiveL2capMtu < negotiatedCocMtu &&
        (fragmentsPerSdu >= msys1BlockCount || fragmentsPerSdu * 2 >= msys1BlockCount || fragmentsPerSdu >= aclFromLlCount)) {
        NIMBLE_LOGW(tag,
                    "L2CAP COC 0x%04X large-SDU risk: negotiated MTU %u requires %lu fragments at L2CAP MTU %u. Current pools (MSYS_1=%lux%lu, ACL_FROM_LL=%lu) may cause ENOMEM/timeouts under load.",
                    psm,
                    negotiatedCocMtu,
                    (unsigned long)fragmentsPerSdu,
                    effectiveL2capMtu,
                    (unsigned long)msys1BlockCount,
                    (unsigned long)msys1BlockSize,
                    (unsigned long)aclFromLlCount);
    }

    if (localSduBufferCount < 3) {
        NIMBLE_LOGW(tag,
                    "L2CAP COC 0x%04X local SDU buffer count is %lu. Very small per-channel SDU pools reduce heap use but can make large MTUs fragile when send/receive work overlaps or callbacks lag.",
                    psm,
                    (unsigned long)localSduBufferCount);
    }
}

enum CallbackKind : uint8_t { Connected, Read, Disconnected };
struct DeferredReadItem {
    NimBLEL2CAPChannel* channel;
    std::vector<uint8_t>* data;
    uint32_t generation;
    uint8_t kind;
};
static QueueHandle_t s_l2capDeferredReadQueue = nullptr;
static TaskHandle_t s_l2capDeferredReadTask = nullptr;
static size_t s_l2capChannelCount = 0;
static constexpr size_t LifecycleSlots = 2 * MYNEWT_VAL(BLE_L2CAP_COC_MAX_NUM);

void deferredReadWorker(void*) {
    DeferredReadItem item{};
    while (true) {
        if (xQueueReceive(s_l2capDeferredReadQueue, &item, portMAX_DELAY) != pdTRUE) { continue; }
        auto* ch = item.channel;
        if (item.generation == ch->m_generation.load()) {
            if (item.kind == Connected) {
                ch->callbacks->onConnect(ch, ch->m_negotiatedMTU.load());
            } else if (item.kind == Read && ch->isConnected()) {
                ch->callbacks->onRead(ch, *item.data);
            } else if (item.kind == Disconnected) {
                // Off the host task: callbacks may join CAN RX or finish a command.
                ch->callbacks->onDisconnect(ch);
                while (ch->m_pendingHostJobs.load() || ch->m_writers.load()) { vTaskDelay(1); }
                ch->m_state.store(NimBLEL2CAPChannel::State::idle);
            }
        }
        delete item.data;
        ch->m_pendingDeferredReads.fetch_sub(1);
    }
}

bool ensureDeferredReadWorker() {
    // Channel construction belongs to the application's initialization task.
    if (s_l2capDeferredReadQueue && s_l2capDeferredReadTask) { return true; }
    s_l2capDeferredReadQueue = xQueueCreate(CONFIG_NIMBLE_CPP_L2CAP_CALLBACK_QUEUE_LENGTH + LifecycleSlots,
                                          sizeof(DeferredReadItem));
    if (!s_l2capDeferredReadQueue) { return false; }
    return xTaskCreate(deferredReadWorker, "nimble_l2cap_cb", CONFIG_NIMBLE_CPP_L2CAP_CALLBACK_TASK_STACK_SIZE,
                       nullptr, CONFIG_NIMBLE_CPP_L2CAP_CALLBACK_TASK_PRIORITY, &s_l2capDeferredReadTask) == pdPASS;
}

void NimBLEL2CAPChannel::dispatchCallback(uint8_t kind, std::vector<uint8_t>* data) {
    // The host task is the only producer. Reserve two lifecycle slots per
    // channel; reconnect is refused until its disconnect callback has finished.
    if (kind == Read && uxQueueSpacesAvailable(s_l2capDeferredReadQueue) <= LifecycleSlots) {
        delete data;
        disconnect();
        return;
    }
    DeferredReadItem item{this, data, m_generation.load(), kind};
    m_pendingDeferredReads.fetch_add(1);
    auto result = xQueueSend(s_l2capDeferredReadQueue, &item, 0);
    assert(result == pdTRUE);
}

struct NimBLEL2CAPChannel::TxCompletion {
    SemaphoreHandle_t ready = xSemaphoreCreateBinary();
    std::atomic<int> status{BLE_HS_EUNKNOWN};
    std::atomic<bool> completed{false};
    ~TxCompletion() { vSemaphoreDelete(ready); }
    void reset() {
        // The whole-PDU mutex allows only one waiter. A timeout closes the
        // session, and accept waits for the old host job before reusing this.
        xSemaphoreTake(ready, 0);
        status.store(BLE_HS_EUNKNOWN);
        completed.store(false);
    }
    void finish(int value) {
        if (completed.exchange(true)) { return; }
        status.store(value);
        xSemaphoreGive(ready);
    }
};
void NimBLEL2CAPChannel::transmitOnHost(ble_npl_event* event) {
    auto* ch = static_cast<NimBLEL2CAPChannel*>(ble_npl_event_get_arg(event));
    auto* buffer = ch->m_txBuffer.exchange(nullptr);
    if (!buffer) { return; }
    if (!ch->isConnected() || ch->m_txGeneration.load() != ch->m_generation.load() || !ch->channel) {
        os_mbuf_free_chain(buffer);
        ch->m_completion->finish(BLE_HS_ENOTCONN);
    } else {
        // Arm before send. An immediate TX_UNSTALLED must not be discarded.
        ch->m_activeSend = ch->m_completion.get();
        int rc = ble_l2cap_send(ch->channel, buffer);
        // Pinned NimBLE consumes the SDU even on errors from continue_tx.
        // Only these two early returns leave ownership with the caller.
        if (rc == BLE_HS_EBADDATA || rc == BLE_HS_EBUSY) { os_mbuf_free_chain(buffer); }
        if (rc != BLE_HS_ESTALLED) {
            ch->m_completion->finish(rc);
            ch->m_activeSend = nullptr;
        }
    }
    ch->m_pendingHostJobs.fetch_sub(1);
}

void NimBLEL2CAPChannel::disconnectOnHost(ble_npl_event* event) {
    auto* ch = static_cast<NimBLEL2CAPChannel*>(ble_npl_event_get_arg(event));
    if (ch->m_activeSend) {
        ch->m_activeSend->finish(BLE_HS_ENOTCONN);
        ch->m_activeSend = nullptr;
    }
    if (ch->channel) { ble_l2cap_disconnect(ch->channel); }
    ch->m_pendingHostJobs.fetch_sub(1);
}

NimBLEL2CAPChannel::NimBLEL2CAPChannel(uint16_t psm, uint16_t mtu, NimBLEL2CAPChannelCallbacks* callbacks)
    : psm(psm), mtu(mtu), callbacks(callbacks) {
    assert(mtu);            // fail here, if MTU is too little
    assert(callbacks);      // fail here, if no callbacks are given
    const bool poolReady = setupMemPool();
    assert(poolReady); // fail here, if the memory pool could not be setup
    const bool workerReady = ensureDeferredReadWorker();
    assert(workerReady);
    ++s_l2capChannelCount;
    m_writeMutex = xSemaphoreCreateMutex();
    assert(m_writeMutex);
    m_completion = std::make_unique<TxCompletion>();
    assert(m_completion->ready);
    ble_npl_event_init(&m_txEvent, transmitOnHost, this);
    ble_npl_event_init(&m_disconnectEvent, disconnectOnHost, this);

    NIMBLE_LOGI(LOG_TAG, "L2CAP COC 0x%04X initialized w/ L2CAP MTU %i", this->psm, this->mtu);
};

NimBLEL2CAPChannel::~NimBLEL2CAPChannel() {
    while (m_pendingDeferredReads.load() || m_pendingHostJobs.load() || m_writers.load()) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    ble_npl_event_deinit(&m_txEvent);
    ble_npl_event_deinit(&m_disconnectEvent);
    teardownMemPool();

    // All producers and callbacks have drained before the last channel is
    // destroyed. Keeping the shared worker would retain its internal stack
    // and queue across a full Bluetooth shutdown.
    if (--s_l2capChannelCount == 0) {
        vTaskDelete(s_l2capDeferredReadTask);
        s_l2capDeferredReadTask = nullptr;
        vQueueDelete(s_l2capDeferredReadQueue);
        s_l2capDeferredReadQueue = nullptr;
    }

    NIMBLE_LOGI(LOG_TAG, "L2CAP COC 0x%04X shutdown and freed.", this->psm);
}

bool NimBLEL2CAPChannel::setupMemPool() {
    const size_t buf_blocks = L2CAP_SDU_BUFFER_COUNT;
    // Pool block size includes both headers. Without this headroom a full
    // local-MTU packet would silently need more than one configured buffer.
    const size_t blockSize = size_t(mtu) + sizeof(os_mbuf) + sizeof(os_mbuf_pkthdr);
    if (blockSize > UINT16_MAX) { return false; }
    NIMBLE_LOGD(LOG_TAG, "Allocating %d L2CAP SDU buffers of %d bytes", buf_blocks, mtu);

    memset(&_coc_mempool, 0, sizeof(_coc_mempool));
    memset(&_coc_mbuf_pool, 0, sizeof(_coc_mbuf_pool));

    _coc_memory = malloc(OS_MEMPOOL_SIZE(buf_blocks, blockSize) * sizeof(os_membuf_t));
    if (_coc_memory == 0) {
        NIMBLE_LOGE(LOG_TAG, "Can't allocate _coc_memory: %d", errno);
        return false;
    }

    auto rc = os_mempool_init(&_coc_mempool, buf_blocks, blockSize, _coc_memory, "appbuf");
    if (rc != 0) {
        NIMBLE_LOGE(LOG_TAG, "Can't os_mempool_init: %d", rc);
        return false;
    }

    auto rc2 = os_mbuf_pool_init(&_coc_mbuf_pool, &_coc_mempool, blockSize, buf_blocks);
    if (rc2 != 0) {
        NIMBLE_LOGE(LOG_TAG, "Can't os_mbuf_pool_init: %d", rc2);
        return false;
    }

    this->receiveBuffer = (uint8_t*)malloc(mtu);
    if (!this->receiveBuffer) {
        NIMBLE_LOGE(LOG_TAG, "Can't malloc receive buffer: %d, %s", errno, strerror(errno));
        return false;
    }

    return true;
}

void NimBLEL2CAPChannel::teardownMemPool() {
    if (m_writeMutex) {
        vSemaphoreDelete(m_writeMutex);
        m_writeMutex = nullptr;
    }
    if (this->callbacks) {
        delete this->callbacks;
        this->callbacks = nullptr;
    }
    if (this->receiveBuffer) {
        free(this->receiveBuffer);
        this->receiveBuffer = nullptr;
    }
    if (_coc_mempool.name) {
        os_mempool_unregister(&_coc_mempool);
        memset(&_coc_mempool, 0, sizeof(_coc_mempool));
        memset(&_coc_mbuf_pool, 0, sizeof(_coc_mbuf_pool));
    }
    if (_coc_memory) {
        free(_coc_memory);
        _coc_memory = nullptr;
    }
}

int NimBLEL2CAPChannel::writeFragment(std::vector<uint8_t>::const_iterator begin, std::vector<uint8_t>::const_iterator end) {
    auto* buffer = os_mbuf_get_pkthdr(&_coc_mbuf_pool, 0);
    if (!buffer) { return BLE_HS_ENOMEM; }
    int rc = os_mbuf_append(buffer, &*begin, end - begin);
    if (rc) { os_mbuf_free_chain(buffer); return rc; }
    auto* completion = m_completion.get();
    completion->reset();
    m_txGeneration.store(m_generation.load());
    m_pendingHostJobs.fetch_add(1);
    m_txBuffer.store(buffer);
    if (xTaskGetCurrentTaskHandle() == m_hostTask.load()) {
        transmitOnHost(&m_txEvent);
        // A host callback must never wait for host progress.
        if (xSemaphoreTake(completion->ready, 0) != pdTRUE) { return BLE_HS_ETIMEOUT; }
    } else {
        // At most one TX event and one disconnect event are outstanding per channel.
        ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &m_txEvent);
        if (xSemaphoreTake(completion->ready, pdMS_TO_TICKS(SendTimeoutMs)) != pdTRUE) { return BLE_HS_ETIMEOUT; }
    }
    return completion->status.load();
}

# if MYNEWT_VAL(BLE_ROLE_CENTRAL)
NimBLEL2CAPChannel* NimBLEL2CAPChannel::connect(NimBLEClient*                client,
                                                uint16_t                     psm,
                                                uint16_t                     mtu,
                                                NimBLEL2CAPChannelCallbacks* callbacks) {
    if (!client->isConnected()) {
        NIMBLE_LOGE(
            LOG_TAG,
            "Client is not connected. Before connecting via L2CAP, a GAP connection must have been established");
        return nullptr;
    };

    auto channel = new NimBLEL2CAPChannel(psm, mtu, callbacks);

    auto sdu_rx = os_mbuf_get_pkthdr(&channel->_coc_mbuf_pool, 0);
    if (!sdu_rx) {
        NIMBLE_LOGE(LOG_TAG, "Can't allocate SDU buffer: %d, %s", errno, strerror(errno));
        delete channel;
        return nullptr;
    }
    auto rc = ble_l2cap_connect(client->getConnHandle(), psm, mtu, sdu_rx, NimBLEL2CAPChannel::handleL2capEvent, channel);
    if (rc != 0) {
        NIMBLE_LOGE(LOG_TAG, "ble_l2cap_connect failed: %d", rc);
        os_mbuf_free_chain(sdu_rx);
        delete channel;
        return nullptr;
    }
    return channel;
}
# endif // MYNEWT_VAL(BLE_ROLE_CENTRAL)

bool NimBLEL2CAPChannel::write(const std::vector<uint8_t>& bytes) {
    const auto generation = m_generation.load();
    m_writers.fetch_add(1);
    struct WriterExit { std::atomic<uint32_t>& count; ~WriterExit() { count.fetch_sub(1); } } exit{m_writers};
    if (!isConnected()) { return false; }
    auto wait = xTaskGetCurrentTaskHandle() == m_hostTask.load() ? 0 : pdMS_TO_TICKS(SendTimeoutMs);
    if (xSemaphoreTake(m_writeMutex, wait) != pdTRUE) { disconnect(); return false; }
    struct Unlock { SemaphoreHandle_t mutex; ~Unlock() { xSemaphoreGive(mutex); } } unlock{m_writeMutex};
    if (!isConnected() || generation != m_generation.load()) { return false; }
    const auto width = m_txSduWidth.load();
    if (!width) { disconnect(); return false; }
    auto start = bytes.begin();
    while (start != bytes.end()) {
        if (!isConnected()) { return false; }
        const auto length = std::min<size_t>(width, bytes.end() - start);
        auto end = start + length;
        if (writeFragment(start, end) != 0) { disconnect(); return false; }
        start = end;
    }
    return true;
}

bool NimBLEL2CAPChannel::disconnect() {
    auto state = m_state.load();
    do {
        if (state == State::idle || state == State::closing) { return false; }
    } while (!m_state.compare_exchange_weak(state, State::closing));
    m_pendingHostJobs.fetch_add(1);
    if (xTaskGetCurrentTaskHandle() == m_hostTask.load()) { disconnectOnHost(&m_disconnectEvent); }
    else { ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &m_disconnectEvent); }
    return true;
}

uint16_t NimBLEL2CAPChannel::getConnHandle() const {
    return m_connHandle.load();
}

// private
int NimBLEL2CAPChannel::handleConnectionEvent(struct ble_l2cap_event* event) {
    m_hostTask = xTaskGetCurrentTaskHandle();
    channel = event->connect.chan;
    m_generation.fetch_add(1);
    m_connHandle.store(ble_l2cap_get_conn_handle(channel));
    m_state.store(State::open);
    struct ble_l2cap_chan_info info;
    int rc = ble_l2cap_get_chan_info(channel, &info);
    if (rc != 0) {
        NIMBLE_LOGE(LOG_TAG, "L2CAP COC 0x%04X connected but ble_l2cap_get_chan_info failed: %d", psm, rc);
        disconnect();
        return rc;
    }
    NIMBLE_LOGI(LOG_TAG,
                "L2CAP COC 0x%04X connected. scid=0x%04X dcid=0x%04X local_l2cap=%d local_coc=%d peer_l2cap=%d peer_coc=%d.",
                psm,
                info.scid,
                info.dcid,
                info.our_l2cap_mtu,
                info.our_coc_mtu,
                info.peer_l2cap_mtu,
                info.peer_coc_mtu);
    logPoolSanityWarning(LOG_TAG, psm, info);
    if (info.our_coc_mtu > 0 && info.peer_coc_mtu > 0 && info.our_coc_mtu > info.peer_coc_mtu) {
        NIMBLE_LOGW(LOG_TAG, "L2CAP COC 0x%04X connected, but local MTU is bigger than remote MTU.", psm);
    }

    uint16_t negotiatedMTU = 0;
    if (info.our_coc_mtu > 0 && info.peer_coc_mtu > 0) {
        negotiatedMTU = info.peer_coc_mtu < info.our_coc_mtu ? info.peer_coc_mtu : info.our_coc_mtu;
    } else if (info.our_l2cap_mtu > 0 && info.peer_l2cap_mtu > 0) {
        negotiatedMTU = info.peer_l2cap_mtu < info.our_l2cap_mtu ? info.peer_l2cap_mtu : info.our_l2cap_mtu;
        NIMBLE_LOGW(LOG_TAG,
                    "L2CAP COC 0x%04X connected with zero CoC MTU info; falling back to L2CAP MTU %u for callback.",
                    psm,
                    negotiatedMTU);
    } else {
        negotiatedMTU = mtu;
        NIMBLE_LOGW(LOG_TAG,
                    "L2CAP COC 0x%04X connected with no peer MTU info; falling back to configured MTU %u for callback.",
                    psm,
                    negotiatedMTU);
    }

    m_negotiatedMTU.store(negotiatedMTU);
    uint16_t txSduWidth = negotiatedMTU;
    m_txSduWidth.store(txSduWidth);
#if CONFIG_NIMBLE_CPP_L2CAP_DEFERRED_READ_CALLBACKS
    dispatchCallback(Connected);
#else
    callbacks->onConnect(this, negotiatedMTU);
#endif
    return 0;
}

int NimBLEL2CAPChannel::handleAcceptEvent(struct ble_l2cap_event* event) {
    NIMBLE_LOGI(LOG_TAG, "L2CAP COC 0x%04X accept.", psm);
    m_hostTask = xTaskGetCurrentTaskHandle();
    if (m_state.load() != State::idle || m_pendingDeferredReads.load() || m_pendingHostJobs.load() || m_writers.load() ||
        !callbacks->shouldAcceptConnection(this)) {
        NIMBLE_LOGI(LOG_TAG, "L2CAP COC 0x%04X refused by delegate.", psm);
        return -1;
    }

    struct os_mbuf* sdu_rx = os_mbuf_get_pkthdr(&_coc_mbuf_pool, 0);
    if (!sdu_rx) {
        NIMBLE_LOGE(LOG_TAG, "L2CAP COC 0x%04X could not allocate receive SDU.", psm);
        return BLE_HS_ENOMEM;
    }

    int rc = ble_l2cap_recv_ready(event->accept.chan, sdu_rx);
    if (rc != 0) {
        NIMBLE_LOGE(LOG_TAG, "L2CAP COC 0x%04X ble_l2cap_recv_ready failed during accept: %d", psm, rc);
        os_mbuf_free_chain(sdu_rx);
        return rc;
    }

    m_state.store(State::accepted);
    return 0;
}

int NimBLEL2CAPChannel::handleDataReceivedEvent(struct ble_l2cap_event* event) {
    NIMBLE_LOGD(LOG_TAG, "L2CAP COC 0x%04X data received.", psm);

    struct os_mbuf* rxd = event->receive.sdu_rx;
    assert(rxd != NULL);

    int rx_len = (int)OS_MBUF_PKTLEN(rxd);
    assert(rx_len <= (int)mtu);

    int res = os_mbuf_copydata(rxd, 0, rx_len, receiveBuffer);
    assert(res == 0);

    NIMBLE_LOGD(LOG_TAG, "L2CAP COC 0x%04X received %d bytes.", psm, rx_len);

    res = os_mbuf_free_chain(rxd);
    assert(res == 0);

    std::vector<uint8_t> incomingData(receiveBuffer, receiveBuffer + rx_len);

    struct os_mbuf* next = os_mbuf_get_pkthdr(&_coc_mbuf_pool, 0);
    assert(next != NULL);

    res = ble_l2cap_recv_ready(channel, next);
    assert(res == 0);

#if CONFIG_NIMBLE_CPP_L2CAP_DEFERRED_READ_CALLBACKS
    dispatchCallback(Read, new std::vector<uint8_t>(std::move(incomingData)));
#else
    if (isConnected()) { callbacks->onRead(this, incomingData); }
#endif

    return 0;
}

int NimBLEL2CAPChannel::handleTxUnstalledEvent(struct ble_l2cap_event* event) {
    if (event->tx_unstalled.chan == channel && m_activeSend) {
        m_activeSend->finish(event->tx_unstalled.status);
        m_activeSend = nullptr;
    }
    return 0;
}

int NimBLEL2CAPChannel::handleDisconnectionEvent(struct ble_l2cap_event* event) {
    if (!channel || event->disconnect.chan != channel) { return 0; }
    // NimBLE invokes this while holding its host lock. Never wait or invoke
    // application cleanup here; that cleanup can join a producer using NimBLE.
    m_state.store(State::closing);
    m_connHandle.store(BLE_HS_CONN_HANDLE_NONE);
    channel = nullptr;
    if (m_activeSend) {
        m_activeSend->finish(BLE_HS_ENOTCONN);
        m_activeSend = nullptr;
    }
    dispatchCallback(Disconnected);
    return 0;
}

/* STATIC */
int NimBLEL2CAPChannel::handleL2capEvent(struct ble_l2cap_event* event, void* arg) {
    NIMBLE_LOGD(LOG_TAG, "handleL2capEvent: handling l2cap event %d", event->type);
    NimBLEL2CAPChannel* self = reinterpret_cast<NimBLEL2CAPChannel*>(arg);

    int returnValue = 0;

    switch (event->type) {
        case BLE_L2CAP_EVENT_COC_CONNECTED:
            returnValue = self->handleConnectionEvent(event);
            break;

        case BLE_L2CAP_EVENT_COC_DISCONNECTED:
            returnValue = self->handleDisconnectionEvent(event);
            break;

        case BLE_L2CAP_EVENT_COC_ACCEPT:
            returnValue = self->handleAcceptEvent(event);
            break;

        case BLE_L2CAP_EVENT_COC_DATA_RECEIVED:
            returnValue = self->handleDataReceivedEvent(event);
            break;

        case BLE_L2CAP_EVENT_COC_TX_UNSTALLED:
            returnValue = self->handleTxUnstalledEvent(event);
            break;

        default:
            NIMBLE_LOGW(LOG_TAG, "Unhandled l2cap event %d", event->type);
            break;
    }

    return returnValue;
}

#endif // #if CONFIG_BT_NIMBLE_ENABLED && MYNEWT_VAL(BLE_L2CAP_COC_MAX_NUM)
