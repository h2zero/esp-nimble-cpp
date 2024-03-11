//
// (C) Dr. Michael 'Mickey' Lauer <mickey@vanille-media.de>
//
#include "NimBLEL2CAPService.h"
#include "NimBLEL2CAPServer.h"

#include "NimBLELog.h"
#include "NimBLEUtils.h"

#include "nimble/nimble_port.h"

// Round-up integer division
#define CEIL_DIVIDE(a, b) (((a) + (b) - 1) / (b))
#define ROUND_DIVIDE(a, b) (((a) + (b) / 2) / (b))

static const char* LOG_TAG = "NimBLEL2CAPService";

NimBLEL2CAPService::NimBLEL2CAPService(uint16_t psm, uint16_t mtu, NimBLEL2CAPServiceCallbacks* callbacks) {

    assert(callbacks != NULL);

    const size_t buf_blocks = CEIL_DIVIDE(mtu, L2CAP_BUF_BLOCK_SIZE) * L2CAP_BUF_SIZE_MTUS_PER_CHANNEL;
    printf("# of buf_blocks: %d\n", buf_blocks);

    int rc = ble_l2cap_create_server(psm, mtu, NimBLEL2CAPService::handleL2capEvent, this);
    if (rc != 0) {
        NIMBLE_LOGE(LOG_TAG, "L2CAP Server creation error: %d, %s", rc, NimBLEUtils::returnCodeToString(rc));
        return;
    }

    _coc_memory = malloc(OS_MEMPOOL_SIZE(buf_blocks, L2CAP_BUF_BLOCK_SIZE) * sizeof(os_membuf_t));
    if (_coc_memory == 0) {
        NIMBLE_LOGE(LOG_TAG, "Can't allocate _coc_memory: %d", errno);
        return;
    }

    rc = os_mempool_init(&_coc_mempool, buf_blocks, L2CAP_BUF_BLOCK_SIZE, _coc_memory, "appbuf");
    if (rc != 0) {
        NIMBLE_LOGE(LOG_TAG, "Can't os_mempool_init: %d, %s", rc, NimBLEUtils::returnCodeToString(rc));
        return;
    }
    rc = os_mbuf_pool_init(&_coc_mbuf_pool, &_coc_mempool, L2CAP_BUF_BLOCK_SIZE, buf_blocks);
    if (rc != 0) {
        NIMBLE_LOGE(LOG_TAG, "Can't os_mbuf_pool_init: %d, %s", rc, NimBLEUtils::returnCodeToString(rc));
        return;
    }

    receiveBuffer = (uint8_t*) malloc(mtu);
    if (receiveBuffer == NULL) {
        NIMBLE_LOGE(LOG_TAG, "Can't malloc receive buffer: %d, %s", errno, NimBLEUtils::returnCodeToString(errno));
    }

    this->psm = psm;
    this->mtu = mtu;
    this->callbacks = callbacks;
    NIMBLE_LOGI(LOG_TAG, "L2CAP COC 0x%04X registered w/ L2CAP MTU %i", this->psm, this->mtu);
}

void NimBLEL2CAPService::write(std::vector<uint8_t>& bytes) {

    struct ble_l2cap_chan_info info;
    ble_l2cap_get_chan_info(channel, &info);
    auto mtu = info.peer_coc_mtu;

    while (!bytes.empty()) {
        auto txd = os_mbuf_get_pkthdr(&_coc_mbuf_pool, 0);
        assert(txd != NULL);

        auto chunk = bytes.size() < mtu ? bytes.size() : mtu;
        auto res = os_mbuf_append(txd, bytes.data(), chunk);
        //auto res = os_mbuf_copyinto(txd, 0, bytes.data(), chunk);
        assert(res == 0);
        res = ble_l2cap_send(channel, txd);
        assert(res == 0 || (res == BLE_HS_ESTALLED));
        std::vector<uint8_t>(bytes.begin() + chunk, bytes.end()).swap(bytes);
        NIMBLE_LOGD(LOG_TAG, "L2CAP COC 0x%04X sent %d bytes.", this->psm, chunk);
    }
}

NimBLEL2CAPService::~NimBLEL2CAPService() {
}

// private
int NimBLEL2CAPService::handleConnectionEvent(struct ble_l2cap_event* event) {

    channel = event->connect.chan;
    struct ble_l2cap_chan_info info;
    ble_l2cap_get_chan_info(channel, &info);
    NIMBLE_LOGI(LOG_TAG, "L2CAP COC 0x%04X connected. Our MTU is %i, remote MTU is %i.", psm, info.our_l2cap_mtu, info.peer_l2cap_mtu);
    callbacks->onConnect(this);
    return 0;
}

int NimBLEL2CAPService::handleAcceptEvent(struct ble_l2cap_event* event) {
    NIMBLE_LOGI(LOG_TAG, "L2CAP COC 0x%04X accept.", psm);
    struct os_mbuf *sdu_rx = os_mbuf_get_pkthdr(&_coc_mbuf_pool, 0);
    assert(sdu_rx != NULL);
    ble_l2cap_recv_ready(event->accept.chan, sdu_rx);
    return 0;
}

int NimBLEL2CAPService::handleDataReceivedEvent(struct ble_l2cap_event* event) {
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
    callbacks->onRead(this, incomingData);

    struct os_mbuf* next = os_mbuf_get_pkthdr(&_coc_mbuf_pool, 0);
    assert(next != NULL);

    res = ble_l2cap_recv_ready(channel, next);
    assert(res == 0);

    return 0;
}

int NimBLEL2CAPService::handleTxUnstalledEvent(struct ble_l2cap_event* event) {
    NIMBLE_LOGI(LOG_TAG, "L2CAP COC 0x%04X transmit unstalled.", psm);
    return 0;
}

int NimBLEL2CAPService::handleDisconnectionEvent(struct ble_l2cap_event* event) {
    NIMBLE_LOGI(LOG_TAG, "L2CAP COC 0x%04X disconnected.", psm);
    channel = NULL;
    callbacks->onDisconnect(this);
    return 0;
}

/* STATIC */
int NimBLEL2CAPService::handleL2capEvent(struct ble_l2cap_event *event, void *arg) {

    NIMBLE_LOGD(LOG_TAG, "handleL2capEvent: handling l2cap event %d", event->type);
    NimBLEL2CAPService* self = reinterpret_cast<NimBLEL2CAPService*>(arg);

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

NimBLEL2CAPServiceCallbacks::~NimBLEL2CAPServiceCallbacks() {}
