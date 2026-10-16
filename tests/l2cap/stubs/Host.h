#pragma once
// Minimal host replacements; tests compile the production channel source.
#define NIMBLE_CPP_CLIENT_H_
#define NIMBLE_CPP_LOG_H_
#define NIMBLE_CPP_UTILS_H_
#define NIMBLE_LOGI(...) ((void)0)
#define NIMBLE_LOGD(...) ((void)0)
#define NIMBLE_LOGW(...) ((void)0)
#define NIMBLE_LOGE(...) ((void)0)
#define CONFIG_BT_NIMBLE_ENABLED 1
#define MYNEWT_VAL(x) MYNEWT_VAL_##x
#define MYNEWT_VAL_BLE_L2CAP_COC_MAX_NUM 1
#define MYNEWT_VAL_BLE_ROLE_CENTRAL 0
#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <vector>
struct os_mempool {};
struct os_mbuf_pool {};
using os_membuf_t = uint8_t;
struct os_mbuf { std::vector<uint8_t> bytes; };
#define OS_MEMPOOL_SIZE(n,s) ((n)*(s))
#define OS_MBUF_PKTLEN(m) ((m)->bytes.size())
inline int liveMbufs=0, appendError=0;
inline int os_mempool_init(os_mempool*,size_t,size_t,void*,const char*) { return 0; }
inline int os_mbuf_pool_init(os_mbuf_pool*,os_mempool*,size_t,size_t) { return 0; }
inline os_mbuf* os_mbuf_get_pkthdr(os_mbuf_pool*,int) { ++liveMbufs; return new os_mbuf; }
inline int os_mbuf_append(os_mbuf* b,const void* p,size_t n) {
    if (appendError) return appendError;
    auto a=static_cast<const uint8_t*>(p); b->bytes.insert(b->bytes.end(),a,a+n); return 0;
}
inline int os_mbuf_free_chain(os_mbuf* b) { --liveMbufs; delete b; return 0; }
inline int os_mbuf_copydata(os_mbuf* b,size_t off,size_t n,void* p) { memcpy(p,b->bytes.data()+off,n); return 0; }
constexpr int BLE_HS_EAGAIN=1, BLE_HS_ENOMEM=6, BLE_HS_ENOTCONN=7, BLE_HS_EINVAL=3,
 BLE_HS_EALREADY=2, BLE_HS_EBADDATA=10, BLE_HS_EBUSY=15, BLE_HS_EREJECT=14,
 BLE_HS_ESTALLED=31, BLE_HS_EUNKNOWN=17;
constexpr uint16_t BLE_HS_CONN_HANDLE_NONE=0xffff;
constexpr uint32_t BLE_NPL_TIME_FOREVER=UINT32_MAX;
inline uint32_t ble_npl_time_ms_to_ticks32(uint32_t ms) { return ms; }
inline void ble_npl_time_delay(uint32_t) {} // Retry count, not elapsed radio time, is tested.
struct ble_l2cap_chan {};
struct ble_l2cap_chan_info { uint16_t peer_coc_mtu=8,our_coc_mtu=8,peer_l2cap_mtu=8,our_l2cap_mtu=8; };
constexpr int BLE_L2CAP_EVENT_COC_CONNECTED=1,BLE_L2CAP_EVENT_COC_DISCONNECTED=2,
 BLE_L2CAP_EVENT_COC_ACCEPT=3,BLE_L2CAP_EVENT_COC_DATA_RECEIVED=4,BLE_L2CAP_EVENT_COC_TX_UNSTALLED=5;
struct ble_l2cap_event {
 int type=0;
 struct { ble_l2cap_chan* chan; } connect{},accept{},disconnect{};
 struct { os_mbuf* sdu_rx; } receive{};
 struct { int status; ble_l2cap_chan* chan; } tx_unstalled{};
};
inline std::function<int(os_mbuf*)> hostSend;
inline int hostDisconnects=0;
inline int ble_l2cap_get_chan_info(ble_l2cap_chan*,ble_l2cap_chan_info* i) { *i={}; return 0; }
inline uint16_t ble_l2cap_get_conn_handle(ble_l2cap_chan*) { return 1; }
inline int ble_l2cap_send(ble_l2cap_chan*,os_mbuf* b) { return hostSend(b); }
inline int ble_l2cap_disconnect(ble_l2cap_chan*) { ++hostDisconnects; return 0; }
inline int ble_l2cap_recv_ready(ble_l2cap_chan*,os_mbuf* b) { return os_mbuf_free_chain(b); }
class NimBLEUtils {
 public:
 struct TaskData { mutable int m_flags=0; };
 static bool taskWait(const TaskData&,uint32_t) { assert(false && "credit wait not exercised by send error tests"); return false; }
 static void taskRelease(const TaskData&,int=0) {}
 static const char* returnCodeToString(int) { return "stub"; }
};
