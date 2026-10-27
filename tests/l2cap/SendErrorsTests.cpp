#include "NimBLEL2CAPChannel.h"
#include <iostream>
#include <stdexcept>

static void require(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
struct Channel : NimBLEL2CAPChannel {
    ble_l2cap_chan native;
    Channel() : NimBLEL2CAPChannel(129,64,new NimBLEL2CAPChannelCallbacks) {
        ble_l2cap_event e{};e.connect.chan=&native;handleConnectionEvent(&e);
    }
};
int main(int argc,char** argv) {
    try {
        const std::string test=argc>1?argv[1]:"errors";
        int sends=0;
        if (test=="errors" || test=="ownership") {
            const std::vector<int> codes=test=="errors" ?
                std::vector<int>{BLE_HS_ENOMEM,BLE_HS_EAGAIN,BLE_HS_ENOTCONN,BLE_HS_EINVAL} :
                std::vector<int>{BLE_HS_EBADDATA,BLE_HS_EBUSY};
            for (int error:codes) {
                Channel c;sends=0;hostDisconnects=0;
                hostSend=[&](os_mbuf* b) {
                    ++sends;
                    if (error!=BLE_HS_EBADDATA && error!=BLE_HS_EBUSY) os_mbuf_free_chain(b);
                    return error;
                };
                require(!c.write(std::vector<uint8_t>(20,0x55)),"positive error reported success");
                require(sends==1,"failed SDU was retried or later fragment submitted");
                require(hostDisconnects==1,"failed stream was not disconnected");
                require(liveMbufs==0,"send buffer leaked or freed twice");
            }
        } else if (test=="partial") {
            Channel c;std::vector<uint8_t> delivered;
            hostSend=[&](os_mbuf* b) {
                ++sends;
                const size_t length=sends==1?b->bytes.size():2;
                delivered.insert(delivered.end(),b->bytes.begin(),b->bytes.begin()+length);
                os_mbuf_free_chain(b);return sends==1?0:BLE_HS_ENOMEM;
            };
            require(!c.write(std::vector<uint8_t>(24,0x55)),"partially sent stream reported success");
            require(sends==2 && delivered.size()==10,"partial SDU prefix replayed or remaining fragment sent");
            require(hostDisconnects==1 && liveMbufs==0,"partial-send abort/ownership failed");
        } else if (test=="append") {
            Channel c;appendError=BLE_HS_ENOMEM;
            hostSend=[&](os_mbuf*) { ++sends;return 0; };
            require(!c.write({1,2,3}),"append failure reported success");
            require(sends==0 && liveMbufs==0 && hostDisconnects==1,"append error leaked buffer or submitted data");
        } else if (test=="success") {
            Channel c;std::vector<uint8_t> received;std::vector<uint8_t> input;
            for (int i=0;i<20;++i) input.push_back(uint8_t(i));
            hostSend=[&](os_mbuf* b) {++sends;received.insert(received.end(),b->bytes.begin(),b->bytes.end());return os_mbuf_free_chain(b);};
            require(c.write(input) && sends==3 && received==input && liveMbufs==0 && hostDisconnects==0,"valid fragmented write failed");
        } else if (test=="stalled") {
            Channel c;
            hostSend=[&](os_mbuf* b) {++sends;os_mbuf_free_chain(b);return BLE_HS_ESTALLED;};
            require(c.write({1,2,3}) && sends==1 && liveMbufs==0 && hostDisconnects==0,"accepted stalled SDU incorrectly treated as error");
        } else { throw std::runtime_error("unknown test"); }
        std::cout<<test<<" passed\n";return 0;
    } catch (const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
