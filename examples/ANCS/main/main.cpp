// Original: https://github.com/mathcampbell/ANCS
#include "NimBLEDevice.h"
#include "driver/uart.h"

static NimBLEUUID ancsServiceUUID("7905F431-B5CE-4E99-A40F-4B1E122D00D0");
static NimBLEUUID notificationSourceCharacteristicUUID("9FBF120D-6301-42D9-8C58-25E699A21DBD");
static NimBLEUUID controlPointCharacteristicUUID("69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9");
static NimBLEUUID dataSourceCharacteristicUUID("22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB");

static NimBLEClient *pClient;

uint8_t latestMessageID[4];
bool pendingNotification = false;
bool incomingCall = false;
uint8_t acceptCall = 0;

static void initUart()
{
    uart_config_t uartConfig{};
    uartConfig.baud_rate = 115200;
    uartConfig.data_bits = UART_DATA_8_BITS;
    uartConfig.parity = UART_PARITY_DISABLE;
    uartConfig.stop_bits = UART_STOP_BITS_1;
    uartConfig.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uartConfig.source_clk = UART_SCLK_DEFAULT;

    uart_driver_install(UART_NUM_0, 256, 0, 0, nullptr, 0);
    uart_param_config(UART_NUM_0, &uartConfig);
}

static void dataSourceNotifyCallback(NimBLERemoteCharacteristic *pDataSourceCharacteristic,
                                     uint8_t *pData,
                                     size_t length,
                                     bool isNotify)
{
    for (int i = 0; i < length; i++)
    {
        if (i > 7)
        {
            printf("%c", pData[i]);
        }
        else
        {
            printf("%02X ", pData[i]);
        }
    }
    printf("\n");
}

static void NotificationSourceNotifyCallback(NimBLERemoteCharacteristic *pNotificationSourceCharacteristic,
                                             uint8_t *pData,
                                             size_t length,
                                             bool isNotify)
{
    if (pData[0] == 0)
    {
        printf("New notification!\n");
        latestMessageID[0] = pData[4];
        latestMessageID[1] = pData[5];
        latestMessageID[2] = pData[6];
        latestMessageID[3] = pData[7];

        switch (pData[2])
        {
        case 0:
            printf("Category: Other\n");
            break;
        case 1:
            incomingCall = true;
            printf("Category: Incoming call\n");
            break;
        case 2:
            printf("Category: Missed call\n");
            break;
        case 3:
            printf("Category: Voicemail\n");
            break;
        case 4:
            printf("Category: Social\n");
            break;
        case 5:
            printf("Category: Schedule\n");
            break;
        case 6:
            printf("Category: Email\n");
            break;
        case 7:
            printf("Category: News\n");
            break;
        case 8:
            printf("Category: Health\n");
            break;
        case 9:
            printf("Category: Business\n");
            break;
        case 10:
            printf("Category: Location\n");
            break;
        case 11:
            printf("Category: Entertainment\n");
            break;
        default:
            break;
        }
    }
    else if (pData[0] == 1)
    {
        printf("Notification Modified!\n");
        if (pData[2] == 1)
        {
            printf("Call Changed!\n");
        }
    }
    else if (pData[0] == 2)
    {
        printf("Notification Removed!\n");
        if (pData[2] == 1)
        {
            printf("Call Gone!\n");
        }
    }
    pendingNotification = true;
}

class ServerCallbacks : public NimBLEServerCallbacks
{
    void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo)
    {
        printf("Client connected: %s\n", connInfo.getAddress().toString().c_str());
        pClient = pServer->getClient(connInfo);
        printf("Client connected!\n");
    }

    void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason)
    {
        printf("Client disconnected: %s, reason: %d\n", connInfo.getAddress().toString().c_str(), reason);
    }
} serverCallbacks;

extern "C" void app_main()
{
    initUart();
    printf("Starting setup...\n");

    NimBLEDevice::init("ANCS");
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO);
    NimBLEDevice::setPower(9);

    NimBLEServer *pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(&serverCallbacks);
    pServer->advertiseOnDisconnect(true);

    NimBLEAdvertising *pAdvertising = pServer->getAdvertising();
    NimBLEAdvertisementData advData{};
    advData.setFlags(0x06);
    advData.addServiceUUID(ancsServiceUUID);
    pAdvertising->setAdvertisementData(advData);
    pAdvertising->start();

    printf("Advertising started!\n");

    while (1)
    {
        if (pClient != nullptr && pClient->isConnected())
        {
            auto pAncsService = pClient->getService(ancsServiceUUID);
            if (pAncsService == nullptr)
            {
                printf("Failed to find our service UUID: %s\n", ancsServiceUUID.toString().c_str());
                continue;
            }
            // Obtain a reference to the characteristic in the service of the remote BLE server.
            auto pNotificationSourceCharacteristic = pAncsService->getCharacteristic(notificationSourceCharacteristicUUID);
            if (pNotificationSourceCharacteristic == nullptr)
            {
                printf("Failed to find our characteristic UUID: %s\n",
                       notificationSourceCharacteristicUUID.toString().c_str());
                continue;
            }
            // Obtain a reference to the characteristic in the service of the remote BLE server.
            auto pControlPointCharacteristic = pAncsService->getCharacteristic(controlPointCharacteristicUUID);
            if (pControlPointCharacteristic == nullptr)
            {
                printf("Failed to find our characteristic UUID: %s\n",
                       controlPointCharacteristicUUID.toString().c_str());
                continue;
            }
            // Obtain a reference to the characteristic in the service of the remote BLE server.
            auto pDataSourceCharacteristic = pAncsService->getCharacteristic(dataSourceCharacteristicUUID);
            if (pDataSourceCharacteristic == nullptr)
            {
                printf("Failed to find our characteristic UUID: %s\n", dataSourceCharacteristicUUID.toString().c_str());
                continue;
            }
            pDataSourceCharacteristic->subscribe(true, dataSourceNotifyCallback);
            pNotificationSourceCharacteristic->subscribe(true, NotificationSourceNotifyCallback);

            while (1)
            {
                if (pendingNotification || incomingCall)
                {
                    // CommandID: CommandIDGetNotificationAttributes
                    // 32bit uid
                    // AttributeID
                    printf("Requesting details...\n");
                    uint8_t val[8] =
                        {0x0, latestMessageID[0], latestMessageID[1], latestMessageID[2], latestMessageID[3], 0x0, 0x0, 0x10};
                    pControlPointCharacteristic->writeValue(val, 6, true); // Identifier
                    val[5] = 0x1;
                    pControlPointCharacteristic->writeValue(val, 8, true); // Title
                    val[5] = 0x3;
                    pControlPointCharacteristic->writeValue(val, 8, true); // Message
                    val[5] = 0x5;
                    pControlPointCharacteristic->writeValue(val, 6, true); // Date

                    while (incomingCall)
                    {
                        int bytesRead = uart_read_bytes(UART_NUM_0, &acceptCall, 1, 0);
                        if (bytesRead > 0)
                        {
                            printf("%c\n", (char)acceptCall);
                        }

                        if (acceptCall == 49)
                        { // call accepted , get number 1 from serial
                            const uint8_t vResponse[] =
                                {0x02, latestMessageID[0], latestMessageID[1], latestMessageID[2], latestMessageID[3], 0x00};
                            pControlPointCharacteristic->writeValue((uint8_t *)vResponse, 6, true);

                            acceptCall = 0;
                            // incomingCall = false;
                        }
                        else if (acceptCall == 48)
                        { // call rejected , get number 0 from serial
                            const uint8_t vResponse[] =
                                {0x02, latestMessageID[0], latestMessageID[1], latestMessageID[2], latestMessageID[3], 0x01};
                            pControlPointCharacteristic->writeValue((uint8_t *)vResponse, 6, true);

                            acceptCall = 0;
                            incomingCall = false;
                        }
                        vTaskDelay(10 / portTICK_PERIOD_MS);
                    }

                    pendingNotification = false;
                }
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}