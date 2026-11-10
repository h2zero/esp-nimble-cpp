//
// (C) Dr. Michael 'Mickey' Lauer <mickey@vanille-media.de>
//

#ifndef NIMBLE_CPP_L2CAPCHANNEL_H_
#define NIMBLE_CPP_L2CAPCHANNEL_H_

#include "syscfg/syscfg.h"
#if CONFIG_BT_NIMBLE_ENABLED && MYNEWT_VAL(BLE_L2CAP_COC_MAX_NUM)

# include "inttypes.h"
# ifdef USING_NIMBLE_ARDUINO_HEADERS
#  include "nimble/nimble/host/include/host/ble_l2cap.h"
#  include "nimble/nimble/host/include/host/ble_hs.h"
#  include "nimble/porting/nimble/include/os/os_mbuf.h"
# else
#  include "host/ble_l2cap.h"
#  include "host/ble_hs.h"
#  include "os/os_mbuf.h"
# endif

/****  FIX COMPILATION ****/
# undef min
# undef max
/**************************/

# include "NimBLEUtils.h"

# include <vector>
# include <atomic>
# include <memory>
# ifdef USING_NIMBLE_ARDUINO_HEADERS
#  include "nimble/nimble/include/nimble/nimble_npl.h"
# else
#  include "nimble/nimble_npl.h"
# endif
# include "freertos/FreeRTOS.h"
# include "freertos/semphr.h"
# include "freertos/task.h"

class NimBLEClient;
class NimBLEL2CAPChannelCallbacks;

/**
 * @brief Encapsulates a L2CAP channel.
 *
 * This class is used to encapsulate a L2CAP connection oriented channel, both
 * from the "server" (which waits for the connection to be opened) and the "client"
 * (which opens the connection) point of view.
 */
class NimBLEL2CAPChannel {
  public:
    /// @brief Open an L2CAP channel via the specified PSM and MTU.
    /// @param[in] psm The PSM to use.
    /// @param[in] mtu The MTU to use. Note that this is the local MTU. Upon opening the channel,
    /// the final MTU will be negotiated to be the minimum of local and remote.
    /// @param[in] callbacks The callbacks to use. NOTE that these callbacks are called from the
    /// context of the NimBLE bluetooth task (`nimble_host`) unless deferred callbacks are
    /// enabled. In that mode onConnect()/onRead() run on a shared worker.
    /// onDisconnect() always runs on that worker, serialized after deferred reads.
    /// @return True if the channel was opened successfully, false otherwise.
    static NimBLEL2CAPChannel* connect(NimBLEClient* client, uint16_t psm, uint16_t mtu, NimBLEL2CAPChannelCallbacks* callbacks);

    /// @brief Write data to the channel.
    ///
    /// If the size of the data exceeds the MTU, the data will be split into multiple fragments.
    /// @return true on success, after the data has been sent.
    /// @return false, if the data can't be sent.
    ///
    /// Whole writes are serialized. Credit waits are bounded; an incomplete write
    /// aborts the connection. Call from a worker task for blocking operation.
    /// Owners must disconnect and let callbacks finish before deleting a channel.
    bool write(const std::vector<uint8_t>& bytes);

    /// @brief Disconnect this L2CAP channel.
    /// @return true on success, false on failure.
    bool disconnect();

    /// @brief Get the connection handle associated with this channel.
    /// @return Connection handle, or BLE_HS_CONN_HANDLE_NONE if not connected.
    uint16_t getConnHandle() const;
    /// @return True, if the channel is connected. False, otherwise.
    bool isConnected() const { return m_state.load() == State::open; }
    // Stop accepting new connections before using this to drain a channel.
    bool isIdle() const {
        return m_state.load() == State::idle && !m_pendingDeferredReads.load() &&
               !m_pendingHostJobs.load() && !m_writers.load();
    }

    ~NimBLEL2CAPChannel();

  protected:
    NimBLEL2CAPChannel(uint16_t psm, uint16_t mtu, NimBLEL2CAPChannelCallbacks* callbacks);

    int handleConnectionEvent(struct ble_l2cap_event* event);
    int handleAcceptEvent(struct ble_l2cap_event* event);
    int handleDataReceivedEvent(struct ble_l2cap_event* event);
    int handleTxUnstalledEvent(struct ble_l2cap_event* event);
    int handleDisconnectionEvent(struct ble_l2cap_event* event);

  private:
    friend class NimBLEL2CAPServer;
    friend void deferredReadWorker(void*);
    static constexpr const char* LOG_TAG = "NimBLEL2CAPChannel";

    const uint16_t               psm; // PSM of the channel
    const uint16_t               mtu; // The requested (local) MTU of the channel, might be larger than negotiated MTU
    struct ble_l2cap_chan*       channel = nullptr;
    NimBLEL2CAPChannelCallbacks* callbacks;
    uint8_t*                     receiveBuffer = nullptr; // buffers a full (local) MTU

    // NimBLE memory pool
    void*               _coc_memory = nullptr;
    struct os_mempool   _coc_mempool;
    struct os_mbuf_pool _coc_mbuf_pool;

    // Only host events access the native channel and active SDU. Never hold a
    // wrapper lock while entering NimBLE: disconnect callbacks hold ble_hs_lock.
    enum class State { idle, accepted, open, closing };
    std::atomic<State> m_state{State::idle};
    std::atomic<uint32_t> m_generation{0};
    std::atomic<uint16_t> m_negotiatedMTU{0};
    std::atomic<uint16_t> m_txSduWidth{0};
    std::atomic<uint16_t> m_connHandle{BLE_HS_CONN_HANDLE_NONE};
    std::atomic<uint32_t> m_pendingDeferredReads{0}; // includes running callbacks
    std::atomic<uint32_t> m_pendingHostJobs{0};
    std::atomic<uint32_t> m_writers{0};
    SemaphoreHandle_t m_writeMutex{nullptr};
    struct TxCompletion;
    std::unique_ptr<TxCompletion> m_completion;
    std::atomic<os_mbuf*> m_txBuffer{nullptr};
    std::atomic<uint32_t> m_txGeneration{0};
    TxCompletion* m_activeSend{nullptr}; // host task only
    ble_npl_event m_txEvent{};
    ble_npl_event m_disconnectEvent{};
    std::atomic<TaskHandle_t> m_hostTask{nullptr};
    static void transmitOnHost(ble_npl_event* event);
    static void disconnectOnHost(ble_npl_event* event);
    void dispatchCallback(uint8_t kind, std::vector<uint8_t>* data = nullptr);


    // Allocate / deallocate NimBLE memory pool
    bool setupMemPool();
    void teardownMemPool();

    // Writes data up to the size of the negotiated MTU to the channel.
    int writeFragment(std::vector<uint8_t>::const_iterator begin, std::vector<uint8_t>::const_iterator end);

    // L2CAP event handler
    static int handleL2capEvent(struct ble_l2cap_event* event, void* arg);
};

/**
 * @brief Callbacks base class for the L2CAP channel.
 */
class NimBLEL2CAPChannelCallbacks {
  public:
    NimBLEL2CAPChannelCallbacks()          = default;
    virtual ~NimBLEL2CAPChannelCallbacks() = default;

    /// Called when the client attempts to open a channel on the server.
    /// You can choose to accept or deny the connection.
    /// Default implementation returns true.
    virtual bool shouldAcceptConnection(NimBLEL2CAPChannel* channel) { return true; }
    /// Called after a connection has been made.
    /// Default implementation does nothing.
    virtual void onConnect(NimBLEL2CAPChannel* channel, uint16_t negotiatedMTU) {};
    /// Called when data has been read from the channel.
    /// Default implementation does nothing.
    virtual void onRead(NimBLEL2CAPChannel* channel, std::vector<uint8_t>& data) {};
    /// Called after the channel has been disconnected.
    /// Default implementation does nothing.
    virtual void onDisconnect(NimBLEL2CAPChannel* channel) {};
};

#endif // CONFIG_BT_NIMBLE_ENABLED && MYNEWT_VAL(BLE_L2CAP_COC_MAX_NUM)
#endif // NIMBLE_CPP_L2CAPCHANNEL_H_
