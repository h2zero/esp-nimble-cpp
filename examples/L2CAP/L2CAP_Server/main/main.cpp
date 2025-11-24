#include <NimBLEDevice.h>
#include <esp_hpl.hpp>
#include <esp_timer.h>

#define L2CAP_PSM            192
#define L2CAP_MTU            5000

// Heap monitoring
size_t initialHeap = 0;
size_t lastHeap = 0;
size_t heapDecreaseCount = 0;
const size_t HEAP_LEAK_THRESHOLD = 10;  // Warn after 10 consecutive decreases

class GATTCallbacks: public BLEServerCallbacks {

public:
    void onConnect(BLEServer* pServer, BLEConnInfo& info) {
        /// Booster #1
        pServer->setDataLen(info.getConnHandle(), 251);
        /// Booster #2 (especially for Apple devices)
        BLEDevice::getServer()->updateConnParams(info.getConnHandle(), 12, 12, 0, 200);
    }
};

class L2CAPChannelCallbacks: public BLEL2CAPChannelCallbacks {

public:
    bool connected = false;
    size_t totalBytesReceived = 0;
    size_t totalFramesReceived = 0;
    size_t totalPayloadBytes = 0;
    uint8_t expectedSequenceNumber = 0;
    size_t sequenceErrors = 0;
    size_t frameErrors = 0;
    uint64_t startTime = 0;
    std::vector<uint8_t> buffer;  // Buffer for incomplete frames

public:
    void onConnect(NimBLEL2CAPChannel* channel) {
        printf("L2CAP connection established on PSM %d\n", L2CAP_PSM);
        connected = true;
        totalBytesReceived = 0;
        totalFramesReceived = 0;
        totalPayloadBytes = 0;
        expectedSequenceNumber = 0;
        sequenceErrors = 0;
        frameErrors = 0;
        startTime = esp_timer_get_time();
        buffer.clear();
    }

    void onRead(NimBLEL2CAPChannel* channel, std::vector<uint8_t>& data) {
        // Append new data to buffer
        buffer.insert(buffer.end(), data.begin(), data.end());
        totalBytesReceived += data.size();
        if (startTime == 0) {
            startTime = esp_timer_get_time();  // start measuring once data flows
        }
        
        // Process complete frames from buffer
        while (buffer.size() >= 3) {  // Minimum frame size: seqno(1) + len(2)
            // Parse frame header
            uint8_t seqno = buffer[0];
            uint16_t payloadLen = (buffer[1] << 8) | buffer[2];  // Big-endian
            
            size_t frameSize = 3 + payloadLen;
            
            // Check if we have complete frame
            if (buffer.size() < frameSize) {
                break;  // Wait for more data
            }
            
            // Validate and process frame
            totalFramesReceived++;
            totalPayloadBytes += payloadLen;
            
            // Check sequence number
            if (seqno != expectedSequenceNumber) {
                sequenceErrors++;
                printf("Frame %zu: Sequence error - got %d, expected %d (payload=%d bytes)\n", 
                       totalFramesReceived, seqno, expectedSequenceNumber, payloadLen);
            }
            
            // Update expected sequence number (wraps at 256)
            expectedSequenceNumber = (seqno + 1) & 0xFF;
            
            // Remove processed frame from buffer
            buffer.erase(buffer.begin(), buffer.begin() + frameSize);
            
            // Print progress every 100 frames
            if (totalFramesReceived % 100 == 0) {
                double elapsedSeconds = (esp_timer_get_time() - startTime) / 1000000.0;
                double bytesPerSecond = elapsedSeconds > 0 ? totalBytesReceived / elapsedSeconds : 0.0;
                printf("Received %zu frames (%zu payload bytes) - Bandwidth: %.2f KB/s (%.2f Mbps)\n",
                       totalFramesReceived, totalPayloadBytes,
                       bytesPerSecond / 1024.0, (bytesPerSecond * 8) / 1000000.0);
            }
        }
    }
    
    void onDisconnect(NimBLEL2CAPChannel* channel) {
        printf("\nL2CAP disconnected\n");
        double elapsedSeconds = startTime > 0 ? (esp_timer_get_time() - startTime) / 1000000.0 : 0.0;
        double bytesPerSecond = elapsedSeconds > 0 ? totalBytesReceived / elapsedSeconds : 0.0;

        printf("Final statistics:\n");
        printf("  Total frames: %zu\n", totalFramesReceived);
        printf("  Total bytes: %zu\n", totalBytesReceived);
        printf("  Payload bytes: %zu\n", totalPayloadBytes);
        printf("  Sequence errors: %zu\n", sequenceErrors);
        printf("  Frame errors: %zu\n", frameErrors);
        printf("  Bandwidth: %.2f KB/s (%.2f Mbps)\n", bytesPerSecond / 1024.0, (bytesPerSecond * 8) / 1000000.0);

        // Reset state for the next connection
        buffer.clear();
        totalBytesReceived = 0;
        totalFramesReceived = 0;
        totalPayloadBytes = 0;
        expectedSequenceNumber = 0;
        sequenceErrors = 0;
        frameErrors = 0;
        startTime = 0;
        connected = false;

        // Restart advertising so another client can connect
        BLEDevice::startAdvertising();
    }
};

extern "C"
void app_main(void) {
    // Install high performance logging before any other output
    esp_hpl::HighPerformanceLogger::init();

    printf("Starting L2CAP server example [%lu free] [%lu min]\n", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());

    BLEDevice::init("l2cap");  // Match the name the client is looking for
    BLEDevice::setMTU(BLE_ATT_MTU_MAX);

    auto cocServer = BLEDevice::createL2CAPServer();
    auto l2capChannelCallbacks = new L2CAPChannelCallbacks();
    auto channel = cocServer->createService(L2CAP_PSM, L2CAP_MTU, l2capChannelCallbacks);
    (void)channel;  // prevent unused warning
    
    auto server = BLEDevice::createServer();
    server->setCallbacks(new GATTCallbacks());
    
    auto advertising = BLEDevice::getAdvertising();
    NimBLEAdvertisementData scanData;
    scanData.setName("l2cap");
    advertising->setScanResponseData(scanData);

    BLEDevice::startAdvertising();
    printf("Server waiting for connection requests [%lu free] [%lu min]\n", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());

    // Status reporting loop
    while (true) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        
        if (l2capChannelCallbacks->connected && l2capChannelCallbacks->totalBytesReceived > 0) {
            uint64_t currentTime = esp_timer_get_time();
            double elapsedSeconds = (currentTime - l2capChannelCallbacks->startTime) / 1000000.0;
            
            if (elapsedSeconds > 0) {
                double bytesPerSecond = l2capChannelCallbacks->totalBytesReceived / elapsedSeconds;
                double framesPerSecond = l2capChannelCallbacks->totalFramesReceived / elapsedSeconds;
                
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
                printf("Frames received: %zu (%.1f fps)\n", l2capChannelCallbacks->totalFramesReceived, framesPerSecond);
                printf("Total bytes: %zu\n", l2capChannelCallbacks->totalBytesReceived);
                printf("Payload bytes: %zu\n", l2capChannelCallbacks->totalPayloadBytes);
                printf("Bandwidth: %.2f KB/s (%.2f Mbps)\n", bytesPerSecond / 1024.0, (bytesPerSecond * 8) / 1000000.0);
                printf("Sequence errors: %zu\n", l2capChannelCallbacks->sequenceErrors);
                printf("Heap: %zu free (min: %zu), Used since start: %zu\n", 
                       currentHeap, minHeap, initialHeap > 0 ? initialHeap - currentHeap : 0);
                printf("==================\n");
            }
        }
    }
}
