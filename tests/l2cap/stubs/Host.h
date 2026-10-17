#pragma once
// Host replacements for the radio and RTOS; tests compile the actual channel implementation.
#define NIMBLE_CPP_CLIENT_H_
#define NIMBLE_CPP_LOG_H_
#define NIMBLE_CPP_UTILS_H_
#define NIMBLE_LOGI(...)         ((void)0)
#define NIMBLE_LOGD(...)         ((void)0)
#define NIMBLE_LOGW(...)         ((void)0)
#define NIMBLE_LOGE(...)         ((void)0)
#define CONFIG_BT_NIMBLE_ENABLED 1
#ifndef CONFIG_NIMBLE_CPP_L2CAP_DEFERRED_READ_CALLBACKS
# define CONFIG_NIMBLE_CPP_L2CAP_DEFERRED_READ_CALLBACKS 1
#endif
#define CONFIG_NIMBLE_CPP_L2CAP_CALLBACK_QUEUE_LENGTH    4
#define CONFIG_NIMBLE_CPP_L2CAP_CALLBACK_TASK_STACK_SIZE 4096
#define CONFIG_NIMBLE_CPP_L2CAP_CALLBACK_TASK_PRIORITY   5
#define CONFIG_NIMBLE_CPP_L2CAP_SDU_BUFFER_COUNT         6
#define MYNEWT_VAL(x)                                    MYNEWT_VAL_##x
#define MYNEWT_VAL_BLE_L2CAP_COC_MAX_NUM                 2
#define MYNEWT_VAL_BLE_ROLE_CENTRAL                      0
#define MYNEWT_VAL_MSYS_1_BLOCK_COUNT                    24
#define MYNEWT_VAL_MSYS_1_BLOCK_SIZE                     256
#define MYNEWT_VAL_BLE_TRANSPORT_ACL_FROM_LL_COUNT       24
#define MYNEWT_VAL_BLE_L2CAP_COC_MPS                     256
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
using TickType_t            = uint32_t;
using BaseType_t            = int;
constexpr int        pdTRUE = 1, pdFALSE = 0, pdPASS = 1;
constexpr TickType_t portMAX_DELAY = UINT32_MAX;
#define pdMS_TO_TICKS(x) (x)
struct HostSemaphore {
    std::mutex              mutex;
    std::condition_variable cv;
    bool                    available = false;
};
using SemaphoreHandle_t = HostSemaphore*;
inline SemaphoreHandle_t xSemaphoreCreateBinary() {
    return new HostSemaphore;
}
inline SemaphoreHandle_t xSemaphoreCreateMutex() {
    auto s       = new HostSemaphore;
    s->available = true;
    return s;
}
inline int xSemaphoreTake(SemaphoreHandle_t s, TickType_t timeout) {
    std::unique_lock<std::mutex> l(s->mutex);
    if (!s->cv.wait_for(l, std::chrono::milliseconds(timeout == portMAX_DELAY ? 2000 : timeout), [&] {
            return s->available;
        }))
        return pdFALSE;
    s->available = false;
    return pdTRUE;
}
inline int xSemaphoreGive(SemaphoreHandle_t s) {
    std::lock_guard<std::mutex> l(s->mutex);
    s->available = true;
    s->cv.notify_one();
    return pdTRUE;
}
inline void vSemaphoreDelete(SemaphoreHandle_t s) {
    delete s;
}
inline void vTaskDelay(uint32_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
inline void ble_npl_time_delay(uint32_t ms) {
    vTaskDelay(ms);
}
inline uint32_t ble_npl_time_ms_to_ticks32(uint32_t ms) {
    return ms;
}
struct os_mempool {
    const char* name = nullptr;
};
struct os_mbuf_pool {};
struct os_mbuf_pkthdr { uint32_t length; };
inline size_t hostPayloadCapacity = 0;
using os_membuf_t = uint8_t;
inline std::atomic<int> liveMbufs{0};
inline std::atomic<int> appendError{0};
struct os_mbuf {
    std::vector<uint8_t> bytes;
    os_mbuf(std::vector<uint8_t> data = {}) : bytes(std::move(data)) { ++liveMbufs; }
    ~os_mbuf() { --liveMbufs; }
};
#define OS_MEMPOOL_SIZE(n, s) ((n) * (s))
#define OS_MBUF_PKTLEN(m)     ((m)->bytes.size())
inline int os_mempool_init(os_mempool* p, size_t, size_t, void*, const char* n) {
    p->name = n;
    return 0;
}
inline int os_mbuf_pool_init(os_mbuf_pool*, os_mempool*, size_t blockSize, size_t) {
    hostPayloadCapacity = blockSize - sizeof(os_mbuf) - sizeof(os_mbuf_pkthdr);
    return 0;
}
inline void     os_mempool_unregister(os_mempool*) {}
inline os_mbuf* os_mbuf_get_pkthdr(os_mbuf_pool*, int) {
    return new os_mbuf;
}
inline int os_mbuf_append(os_mbuf* b, const void* p, size_t n) {
    if (appendError.load()) return appendError.load();
    auto a = (const uint8_t*)p;
    b->bytes.insert(b->bytes.end(), a, a + n);
    return 0;
}
inline int os_mbuf_free_chain(os_mbuf* b) {
    delete b;
    return 0;
}
inline int os_mbuf_copydata(os_mbuf* b, size_t off, size_t n, void* p) {
    memcpy(p, b->bytes.data() + off, n);
    return 0;
}
constexpr int      BLE_HS_EAGAIN = 1, BLE_HS_ENOMEM = 6, BLE_HS_ENOTCONN = 7, BLE_HS_EINVAL = 3, BLE_HS_EALREADY = 2,
                   BLE_HS_EBADDATA = 10, BLE_HS_EBUSY = 15, BLE_HS_EREJECT = 14, BLE_HS_ESTALLED = 31, BLE_HS_EUNKNOWN = 17,
                   BLE_HS_ETIMEOUT         = 13;
constexpr uint16_t BLE_HS_CONN_HANDLE_NONE = 0xffff;
struct ble_l2cap_chan {};
struct ble_l2cap_chan_info {
    uint16_t peer_coc_mtu = 8, our_coc_mtu = 8, peer_l2cap_mtu = 8, our_l2cap_mtu = 8, scid = 1, dcid = 2, psm = 129;
};
constexpr int BLE_L2CAP_EVENT_COC_CONNECTED = 1, BLE_L2CAP_EVENT_COC_DISCONNECTED = 2, BLE_L2CAP_EVENT_COC_ACCEPT = 3,
              BLE_L2CAP_EVENT_COC_DATA_RECEIVED = 4, BLE_L2CAP_EVENT_COC_TX_UNSTALLED = 5;
struct ble_l2cap_event {
    int type = 0;
    struct {
        ble_l2cap_chan* chan;
    } connect{}, accept{}, disconnect{};
    struct {
        os_mbuf* sdu_rx;
    } receive{};
    struct {
        int             status;
        ble_l2cap_chan* chan;
    } tx_unstalled{};
};
inline std::function<int(os_mbuf*)> hostSend;
inline std::atomic<int>             hostDisconnects{0};
inline int                          ble_l2cap_get_chan_info(ble_l2cap_chan* c, ble_l2cap_chan_info* i) {
    if (!c) return BLE_HS_ENOTCONN;
    *i = {};
    return 0;
}
inline uint16_t ble_l2cap_get_conn_handle(ble_l2cap_chan*) {
    return 1;
}
inline int ble_l2cap_send(ble_l2cap_chan*, os_mbuf* b) {
    return hostSend(b);
}
inline int ble_l2cap_disconnect(ble_l2cap_chan*) {
    ++hostDisconnects;
    return 0;
}
inline int ble_l2cap_recv_ready(ble_l2cap_chan*, os_mbuf* b) {
    delete b;
    return 0;
}
class NimBLEUtils {
  public:
    static const char* returnCodeToString(int) { return "stub"; }
};
#include <deque>
using TaskHandle_t = void*;
struct HostTaskExit {};
struct HostTask {
    std::atomic<bool> cancelled{false};
    std::thread thread;
};
inline thread_local HostTask* currentHostTask = nullptr;
inline std::atomic<int> hostTaskCount{0};
inline std::atomic<int> hostQueueCount{0};
inline TaskHandle_t xTaskGetCurrentTaskHandle() {
    thread_local int token;
    return &token;
}
inline int xTaskCreate(void (*fn)(void*), const char*, uint32_t, void* arg, int, TaskHandle_t* handle) {
    auto task = new HostTask;
    *handle = task;
    ++hostTaskCount;
    task->thread = std::thread([=] {
        currentHostTask = task;
        try { fn(arg); } catch (const HostTaskExit&) {}
    });
    return pdPASS;
}
inline void vTaskDelete(TaskHandle_t handle) {
    auto task = static_cast<HostTask*>(handle);
    assert(task && task != currentHostTask);
    task->cancelled = true;
    task->thread.join();
    delete task;
    --hostTaskCount;
}
struct HostQueue {
    std::mutex                       mutex;
    std::condition_variable          cv;
    size_t                           capacity, width;
    std::deque<std::vector<uint8_t>> items;
};
using QueueHandle_t = HostQueue*;
inline QueueHandle_t xQueueCreate(size_t n, size_t w) {
    ++hostQueueCount;
    auto q      = new HostQueue;
    q->capacity = n;
    q->width    = w;
    return q;
}
inline void vQueueDelete(QueueHandle_t q) {
    assert(q->items.empty());
    delete q;
    --hostQueueCount;
}
inline size_t uxQueueSpacesAvailable(QueueHandle_t q) {
    std::lock_guard<std::mutex> l(q->mutex);
    return q->capacity - q->items.size();
}
inline int xQueueSend(QueueHandle_t q, const void* item, TickType_t) {
    std::lock_guard<std::mutex> l(q->mutex);
    if (q->items.size() == q->capacity) return pdFALSE;
    auto b = (const uint8_t*)item;
    q->items.emplace_back(b, b + q->width);
    q->cv.notify_one();
    return pdTRUE;
}
inline int xQueueReceive(QueueHandle_t q, void* item, TickType_t) {
    std::unique_lock<std::mutex> l(q->mutex);
    while (q->items.empty()) {
        if (currentHostTask && currentHostTask->cancelled.load()) { throw HostTaskExit{}; }
        q->cv.wait_for(l, std::chrono::milliseconds(1));
    }
    memcpy(item, q->items.front().data(), q->width);
    q->items.pop_front();
    return pdTRUE;
}
struct ble_npl_event {
    void (*fn)(ble_npl_event*) = nullptr;
    void* arg                  = nullptr;
};
inline void ble_npl_event_init(ble_npl_event* e, void (*fn)(ble_npl_event*), void* p) {
    e->fn  = fn;
    e->arg = p;
}
inline void  ble_npl_event_deinit(ble_npl_event*) {}
inline void* ble_npl_event_get_arg(ble_npl_event* e) {
    return e->arg;
}
inline QueueHandle_t nimble_port_get_dflt_eventq() {
    static auto q = [] {
        auto q = xQueueCreate(128, sizeof(ble_npl_event*));
        std::thread([=] {
            for (;;) {
                ble_npl_event* e;
                xQueueReceive(q, &e, portMAX_DELAY);
                e->fn(e);
            }
        }).detach();
        return q;
    }();
    return q;
}
inline void ble_npl_eventq_put(QueueHandle_t q, ble_npl_event* e) {
    const auto queued = xQueueSend(q, &e, 0);
    assert(queued == pdTRUE);
    (void)queued;
}
