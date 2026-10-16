#include "NimBLEL2CAPChannel.h"
#include <future>
#include <iostream>
#include <stdexcept>
using namespace std::chrono_literals;
static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
static void onHost(std::function<void()> fn) {
    struct Call {
        ble_npl_event                       event;
        std::function<void()>               fn;
        std::shared_ptr<std::promise<void>> done;
    };
    auto  done   = std::make_shared<std::promise<void>>();
    auto  future = done->get_future();
    auto* call   = new Call{{}, std::move(fn), done};
    ble_npl_event_init(
        &call->event,
        [](ble_npl_event* e) {
            std::unique_ptr<Call> c(static_cast<Call*>(ble_npl_event_get_arg(e)));
            c->fn();
            c->done->set_value();
        },
        call);
    ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &call->event);
    future.get();
}
struct Channel : NimBLEL2CAPChannel {
    ble_l2cap_chan native;
    Channel(NimBLEL2CAPChannelCallbacks* cb = new NimBLEL2CAPChannelCallbacks) : NimBLEL2CAPChannel(129, 64, cb) {
        onHost([&] {
            ble_l2cap_event e{};
            e.connect.chan = &native;
            handleConnectionEvent(&e);
        });
    }
    ~Channel() { disconnected(); }
    bool accept() {
        int result = -1;
        onHost([&] {
            ble_l2cap_event e{};
            e.accept.chan = &native;
            result        = handleAcceptEvent(&e);
        });
        return result == 0;
    }
    void connected() {
        onHost([&] {
            ble_l2cap_event e{};
            e.connect.chan = &native;
            handleConnectionEvent(&e);
        });
    }
    void unstalled(int status = 0) {
        ble_l2cap_event e{};
        e.tx_unstalled.status = status;
        e.tx_unstalled.chan   = &native;
        handleTxUnstalledEvent(&e);
    }
    void disconnected() {
        onHost([&] {
            ble_l2cap_event e{};
            e.disconnect.chan = &native;
            handleDisconnectionEvent(&e);
        });
    }
    void receive(std::vector<uint8_t> bytes) {
        onHost([&] {
            ble_l2cap_event e{};
            e.receive.sdu_rx = new os_mbuf{std::move(bytes)};
            handleDataReceivedEvent(&e);
        });
    }
};
int main(int argc, char** argv) {
    try {
        const std::string name = argc > 1 ? argv[1] : "errors";
        if (name == "pool") {
            Channel c;
            require(hostPayloadCapacity >= 64, "one configured buffer cannot hold a full local-MTU SDU");
        } else if (name == "lifecycle") {
            nimble_port_get_dflt_eventq();
            const int baselineQueues = hostQueueCount.load();
            for (int cycle = 0; cycle < 30; ++cycle) {
                {
                    Channel first;
                    {
                        Channel second;
                        require(hostTaskCount == 1 && hostQueueCount == baselineQueues + 1, "channels did not share worker");
                    }
                    require(hostTaskCount == 1 && hostQueueCount == baselineQueues + 1, "worker stopped before last channel");
                }
                require(hostTaskCount == 0 && hostQueueCount == baselineQueues, "worker or queue leaked after shutdown");
            }
        } else if (name == "errors") {
            for (auto error : {BLE_HS_ENOMEM, BLE_HS_EAGAIN, BLE_HS_ENOTCONN, BLE_HS_EINVAL}) {
                Channel c;
                int     sends   = 0;
                hostDisconnects = 0;
                hostSend        = [&](os_mbuf* b) {
                    ++sends;
                    delete b;
                    return error;
                };
                const bool success = c.write(std::vector<uint8_t>(20, 0x55));
                require(!success, "positive send error incorrectly reported as success");
                require(sends == 1, "continued after failed PDU fragment");
                onHost([] {});
                require(hostDisconnects == 1, "failed PDU did not abort stream");
            }
        } else if (name == "partial") {
            Channel c;
            int sends = 0;
            std::vector<uint8_t> delivered;
            hostDisconnects = 0;
            hostSend = [&](os_mbuf* b) {
                ++sends;
                const size_t size = sends == 1 ? b->bytes.size() : 2;
                delivered.insert(delivered.end(), b->bytes.begin(), b->bytes.begin() + size);
                delete b;
                return sends == 1 ? 0 : BLE_HS_ENOMEM;
            };
            require(!c.write(std::vector<uint8_t>(24, 0x55)), "partial stream reported success");
            onHost([] {});
            require(sends == 2 && delivered.size() == 10 && hostDisconnects == 1,
                    "partial SDU was replayed or incomplete stream not aborted");
            require(liveMbufs == 0, "partial send leaked mbuf");
        } else if (name == "append") {
            Channel c;
            std::atomic<int> sends{0};
            hostDisconnects = 0;
            appendError = BLE_HS_ENOMEM;
            hostSend = [&](os_mbuf* b) { ++sends; delete b; return 0; };
            require(!c.write({1, 2, 3}), "append error reported success");
            onHost([] {});
            require(sends == 0 && liveMbufs == 0 && hostDisconnects == 1,
                    "append error leaked mbuf or submitted data");
        } else if (name == "success") {
            Channel c;
            int sends = 0;
            std::vector<uint8_t> input, received;
            for (int i = 0; i < 20; ++i) input.push_back(uint8_t(i));
            hostSend = [&](os_mbuf* b) {
                ++sends;
                received.insert(received.end(), b->bytes.begin(), b->bytes.end());
                delete b;
                return 0;
            };
            require(c.write(input) && sends == 3 && received == input && liveMbufs == 0,
                    "valid fragmented write failed");
        } else if (name == "early") {
            Channel c;
            hostSend = [&](os_mbuf* b) {
                delete b;
                c.unstalled();
                return BLE_HS_ESTALLED;
            };
            const auto start  = std::chrono::steady_clock::now();
            bool       result = c.write({1, 2, 3});
            require(result, "early TX_UNSTALLED completion lost");
            require(std::chrono::steady_clock::now() - start < 500ms, "early completion was drained before wait");
        } else if (name == "disconnect") {
            Channel            c;
            std::promise<void> submitted;
            hostSend = [&](os_mbuf* b) {
                delete b;
                submitted.set_value();
                return BLE_HS_ESTALLED;
            };
            auto writer = std::async(std::launch::async, [&] { return c.write({1, 2, 3}); });
            submitted.get_future().wait();
            c.disconnected();
            bool woke = writer.wait_for(300ms) == std::future_status::ready;
            if (!woke)
                onHost([&] { c.unstalled(BLE_HS_ENOTCONN); }); // let old implementation exit before reporting failure
            const bool success = writer.get();
            require(woke, "disconnect did not wake blocked sender");
            require(!success, "disconnected send reported success");
        } else if (name == "serialize") {
            Channel              c;
            std::mutex           mutex;
            std::vector<uint8_t> sent;
            hostSend = [&](os_mbuf* b) {
                std::lock_guard<std::mutex> l(mutex);
                sent.push_back(b->bytes.front());
                delete b;
                vTaskDelay(2);
                return 0;
            };
            auto a = std::async(std::launch::async, [&] { return c.write(std::vector<uint8_t>(80, 1)); });
            auto b = std::async(std::launch::async, [&] { return c.write(std::vector<uint8_t>(80, 2)); });
            require(!c.accept(), "accepted another connection during active writes");
            require(a.get() && b.get(), "concurrent write failed");
            require(sent.size() == 20, "wrong fragment count");
            int changes = 0;
            for (size_t i = 1; i < sent.size(); ++i) changes += sent[i] != sent[i - 1];
            require(changes == 1, "whole-PDU fragments interleaved");
        } else if (name == "timeout") {
            Channel          c;
            std::atomic<int> sends{0};
            hostDisconnects = 0;
            hostSend        = [&](os_mbuf* b) {
                ++sends;
                delete b;
                return BLE_HS_ESTALLED;
            };
            auto start = std::chrono::steady_clock::now();
            require(!c.write(std::vector<uint8_t>(20, 1)), "stalled timeout reported success");
            require(std::chrono::steady_clock::now() - start < 3s, "send timeout unbounded");
            onHost([] {});
            require(hostDisconnects == 1 && sends == 1, "timeout did not abort remaining fragments");
        } else if (name == "queued_timeout") {
            Channel            c;
            std::promise<void> entered, release;
            auto               gate = release.get_future().share();
            std::atomic<int>   sends{0};
            hostDisconnects = 0;
            hostSend        = [&](os_mbuf* b) {
                ++sends;
                delete b;
                return 0;
            };
            auto blocked = std::async(std::launch::async, [&] {
                onHost([&] {
                    entered.set_value();
                    gate.wait();
                });
            });
            entered.get_future().wait();
            bool success = c.write(std::vector<uint8_t>(20, 1));
            release.set_value();
            blocked.get();
            onHost([] {});
            require(!success && sends == 0, "queued SDU was sent after timeout closed session");
            require(hostDisconnects == 1, "queued timeout did not request disconnect");
        } else if (name == "reconnect") {
            Channel c;
            require(!c.accept(), "accepted a second connection while open");
            hostSend = [&](os_mbuf* b) {
                delete b;
                return BLE_HS_ESTALLED;
            };
            require(!c.write({1}), "stalled write did not time out");
            require(!c.accept(), "accepted before native disconnect and cleanup");
            c.disconnected();
            bool accepted = false;
            for (int i = 0; i < 200 && !accepted; ++i) {
                accepted = c.accept();
                if (!accepted) vTaskDelay(1);
            }
            require(accepted, "cleanup did not allow reconnect");
            require(!c.accept(), "accepted twice before connected event");
            c.connected();
            hostSend = [&](os_mbuf* b) {
                delete b;
                return 0;
            };
            require(c.write(std::vector<uint8_t>(80, 2)), "old timeout contaminated new connection");
        } else if (name == "ownership") {
            for (auto error : {BLE_HS_EBADDATA, BLE_HS_EBUSY}) {
                Channel c;
                hostSend = [=](os_mbuf*) { return error; }; // Native early errors retain caller ownership.
                require(!c.write({1}), "early native error reported success");
            }
        } else if (name == "callbacks") {
            struct Callbacks : NimBLEL2CAPChannelCallbacks {
                std::promise<void>       entered, release;
                std::shared_future<void> gate = release.get_future().share();
                std::atomic<int>         reads{0}, closes{0};
                void                     onRead(NimBLEL2CAPChannel*, std::vector<uint8_t>&) override {
                    ++reads;
                    entered.set_value();
                    gate.wait();
                }
                void onDisconnect(NimBLEL2CAPChannel*) override { ++closes; }
            };
            auto*   callbacks = new Callbacks;
            Channel c(callbacks);
            hostDisconnects = 0;
            c.receive({1});
            callbacks->entered.get_future().wait();
            for (int i = 0; i < 8; ++i) c.receive({2});
            require(hostDisconnects == 1, "full callback queue did not abort");
            require(callbacks->reads == 1, "queue overflow invoked an inline read");
            c.disconnected();
            require(callbacks->closes == 0, "disconnect overlapped running read");
            callbacks->release.set_value();
            for (int i = 0; i < 100 && callbacks->closes == 0; ++i) vTaskDelay(1);
            require(callbacks->closes == 1 && callbacks->reads == 1, "stale reads ran after disconnect");
        }
        std::cout << name << " passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
