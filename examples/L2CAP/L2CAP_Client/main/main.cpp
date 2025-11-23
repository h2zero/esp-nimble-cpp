#include <NimBLEDevice.h>
#include <esp_timer.h>

#define L2CAP_PSM            192
#define L2CAP_MTU            5000
#define INITIAL_PAYLOAD_SIZE 64
#define BLOCKS_BEFORE_DOUBLE 50
#define MAX_PAYLOAD_SIZE     1024

const BLEAdvertisedDevice* theDevice = NULL;
BLEClient* theClient = NULL;
BLEL2CAPChannel* theChannel = NULL;

size_t bytesSent = 0;
size_t bytesReceived = 0;
size_t currentPayloadSize = INITIAL_PAYLOAD_SIZE;
uint32_t blocksSent = 0;
uint64_t startTime = 0;

// Heap monitoring
size_t initialHeap = 0;
size_t lastHeap = 0;
size_t heapDecreaseCount = 0;
const size_t HEAP_LEAK_THRESHOLD = 10;  // Warn after 10 consecutive decreases

class L2CAPChannelCallbacks: public BLEL2CAPChannelCallbacks {

public:
    void onConnect(NimBLEL2CAPChannel* channel) {
        printf("L2CAP connection established\n");
    }

    void onMTUChange(NimBLEL2CAPChannel* channel, uint16_t mtu) {
        printf("L2CAP MTU changed to %d\n", mtu);
    }

    void onRead(NimBLEL2CAPChannel* channel, std::vector<uint8_t>& data) {
        printf("L2CAP read %d bytes\n", data.size());
    }
    void onDisconnect(NimBLEL2CAPChannel* channel) {
        printf("L2CAP disconnected\n");
    }
};

class MyClientCallbacks: public BLEClientCallbacks {

    void onConnect(BLEClient* pClient) {
        printf("GAP connected\n");
        pClient->setDataLen(251);

        theChannel = BLEL2CAPChannel::connect(pClient, L2CAP_PSM, L2CAP_MTU, new L2CAPChannelCallbacks());
    }

    void onDisconnect(BLEClient* pClient, int reason) {
        printf("GAP disconnected (reason: %d)\n", reason);
        theDevice = NULL;
        theChannel = NULL;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        BLEDevice::getScan()->start(5 * 1000, true);
    }
};

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {

    void onResult(const BLEAdvertisedDevice* advertisedDevice) {
        if (theDevice) { return; }
        printf("BLE Advertised Device found: %s\n", advertisedDevice->toString().c_str());

        // Look for device named "l2cap"
        if (advertisedDevice->haveName() && advertisedDevice->getName() == "l2cap") {
            printf("Found l2cap device!\n");
            BLEDevice::getScan()->stop();
            theDevice = advertisedDevice;
        }
    }
};

void statusTask(void *pvParameters) {
    while (true) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        
        if (startTime > 0 && blocksSent > 0) {
            uint64_t currentTime = esp_timer_get_time();
            double elapsedSeconds = (currentTime - startTime) / 1000000.0;
            double bytesPerSecond = bytesSent / elapsedSeconds;
            double kbPerSecond = bytesPerSecond / 1024.0;
            
            // Heap monitoring
            size_t currentHeap = esp_get_free_heap_size();
            size_t minHeap = esp_get_minimum_free_heap_size();
            
            // Track heap for leak detection
            if (initialHeap == 0) {
                initialHeap = currentHeap;
                lastHeap = currentHeap;
            }
            
            // Check for consistent heap decrease
            if (currentHeap < lastHeap) {
                heapDecreaseCount++;
                if (heapDecreaseCount >= HEAP_LEAK_THRESHOLD) {
                    printf("\n⚠️  WARNING: POSSIBLE MEMORY LEAK DETECTED! ⚠️\n");
                    printf("Heap has decreased %zu times in a row\n", heapDecreaseCount);
                    printf("Initial heap: %zu, Current heap: %zu, Lost: %zu bytes\n", 
                           initialHeap, currentHeap, initialHeap - currentHeap);
                }
            } else if (currentHeap >= lastHeap) {
                heapDecreaseCount = 0;  // Reset counter if heap stabilizes or increases
            }
            lastHeap = currentHeap;
            
            printf("\n=== STATUS UPDATE ===\n");
            printf("Blocks sent: %lu\n", (unsigned long)blocksSent);
            printf("Total bytes sent: %zu\n", bytesSent);
            printf("Current payload size: %zu bytes\n", currentPayloadSize);
            printf("Elapsed time: %.1f seconds\n", elapsedSeconds);
            printf("Bandwidth: %.2f KB/s (%.2f Mbps)\n", kbPerSecond, (bytesPerSecond * 8) / 1000000.0);
            printf("Heap: %zu free (min: %zu), Used since start: %zu\n", 
                   currentHeap, minHeap, initialHeap > 0 ? initialHeap - currentHeap : 0);
            printf("==================\n\n");
        }
    }
}

void connectTask(void *pvParameters) {

    uint8_t sequenceNumber = 0;

    while (true) {
        
        if (!theDevice) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        if (!theClient) {
            theClient = BLEDevice::createClient();
            theClient->setConnectionParams(6, 6, 0, 42);

            auto callbacks = new MyClientCallbacks();
            theClient->setClientCallbacks(callbacks);

            auto success = theClient->connect(theDevice);
            if (!success) {
                printf("Error: Could not connect to device\n");
                break;
            }
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            continue;            
        }

        if (!theChannel) {
            printf("l2cap channel not initialized\n");
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            continue;
        }

        if (!theChannel->isConnected()) {
            printf("l2cap channel not connected\n");
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            continue;
        }

        while (theChannel->isConnected()) {
            // Create framed packet: [seqno 8bit] [16bit payload length] [payload]
            std::vector<uint8_t> packet;
            packet.reserve(3 + currentPayloadSize);
            
            // Add sequence number (8 bits)
            packet.push_back(sequenceNumber);
            
            // Add payload length (16 bits, big endian - network byte order)
            uint16_t payloadLen = currentPayloadSize;
            packet.push_back((payloadLen >> 8) & 0xFF);  // High byte first
            packet.push_back(payloadLen & 0xFF);         // Low byte second
            
            // Add payload
            for (size_t i = 0; i < currentPayloadSize; i++) {
                packet.push_back(i & 0xFF);
            }
            
            if (theChannel->write(packet)) {
                if (startTime == 0) {
                    startTime = esp_timer_get_time();
                }
                bytesSent += packet.size();
                blocksSent++;
                
                // Print every block since we're sending slowly now
                printf("Sent block %lu (seq=%d, payload=%zu bytes, frame_size=%zu)\n", 
                       (unsigned long)blocksSent, sequenceNumber, currentPayloadSize, packet.size());
                
                sequenceNumber++;
                
                // After every 50 blocks, double payload size
                if (blocksSent % BLOCKS_BEFORE_DOUBLE == 0) {
                    size_t newSize = currentPayloadSize * 2;
                    
                    // Cap at maximum safe payload size
                    if (newSize > MAX_PAYLOAD_SIZE) {
                        if (currentPayloadSize < MAX_PAYLOAD_SIZE) {
                            currentPayloadSize = MAX_PAYLOAD_SIZE;
                            printf("\n=== Reached maximum payload size of %zu bytes after %lu blocks ===\n", currentPayloadSize, (unsigned long)blocksSent);
                        }
                        // Already at max, don't increase further
                    } else {
                        currentPayloadSize = newSize;
                        printf("\n=== Doubling payload size to %zu bytes after %lu blocks ===\n", currentPayloadSize, (unsigned long)blocksSent);
                    }
                }
            } else {
                printf("failed to send!\n");
                abort();                
            }
            
            // No delay - send as fast as possible
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

extern "C"
void app_main(void) {
    printf("Starting L2CAP client example\n");

    xTaskCreate(connectTask, "connectTask", 5000, NULL, 1, NULL);
    xTaskCreate(statusTask, "statusTask", 3000, NULL, 1, NULL);

    BLEDevice::init("L2CAP-Client");
    BLEDevice::setMTU(BLE_ATT_MTU_MAX);

    auto scan = BLEDevice::getScan();
    auto callbacks = new MyAdvertisedDeviceCallbacks();
    scan->setScanCallbacks(callbacks);
    scan->setInterval(1349);
    scan->setWindow(449);
    scan->setActiveScan(true);
    scan->start(25 * 1000, false);

    // Main task just waits
    while (true) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
