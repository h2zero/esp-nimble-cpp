/*
 * Copyright 2020-2024 Ryan Powell <ryan@nable-embedded.io> and
 * esp-nimble-cpp, NimBLE-Arduino contributors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NIMBLE_CPP_UTILS_H_
#define NIMBLE_CPP_UTILS_H_

#include "nimconfig.h"
#if defined(CONFIG_BT_ENABLED)
# include "NimBLEUUID.h"
# include "functional"
# include <string>

class NimBLEAddress;

/**
 * @brief A structure to hold data for a task that is waiting for a response.
 * @details This structure is used in conjunction with NimBLEUtils::taskWait() and NimBLEUtils::taskRelease().
 * All items are optional, the m_pHandle will be set in taskWait().
 */
struct NimBLETaskData {
    NimBLETaskData(void* pInstance = nullptr, int flags = 0, void* buf = nullptr);
    ~NimBLETaskData();
    void*       m_pInstance{nullptr};
    mutable int m_flags{0};
    void*       m_pBuf{nullptr};

  private:
    mutable void* m_pHandle{nullptr}; // semaphore or task handle
    friend class NimBLEUtils;
};

/**
 * @brief A BLE Utility class with methods for debugging and general purpose use.
 */
class NimBLEUtils {
  public:
    static const char*   gapEventToString(uint8_t eventType);
    static std::string   dataToHexString(const uint8_t* source, uint8_t length);
    static const char*   advTypeToString(uint8_t advType);
    static const char*   returnCodeToString(int rc);
    static NimBLEAddress generateAddr(bool nrpa);
    static bool          taskWait(const NimBLETaskData& taskData, uint32_t timeout);
    static void          taskRelease(const NimBLETaskData& taskData, int rc = 0);

    template <typename T, typename S>
    static void getAttr(const NimBLEUUID& uuid, T* attr, const std::vector<S*> vec, const std::function<bool(const NimBLEUUID*, void*)>& getter) {
        // Check if already exists.
        for (const auto& v : vec) {
            if (v->getUUID() == uuid) {
                attr = v;
                return;
            }
        }

        // Exit if request failed or uuid was found.
        if (!getter(&uuid, attr) || attr) {
            return;
        }

        // Try again with 128 bit uuid if request succeeded with no uuid found.
        if (uuid.bitSize() == BLE_UUID_TYPE_16 || uuid.bitSize() == BLE_UUID_TYPE_32) {
            NimBLEUUID uuid128 = NimBLEUUID(uuid).to128();
            getter(&uuid128, attr);
            return;
        }
        // Try again with 16 bit uuid if request succeeded with no uuid found.
        // If the uuid was 128 bit but not of the BLE base type this check will fail.
        NimBLEUUID uuid16 = NimBLEUUID(uuid).to16();
        if (uuid16.bitSize() == BLE_UUID_TYPE_16) {
            getter(&uuid16, attr);
        }
    }
};

#endif // CONFIG_BT_ENABLED
#endif // NIMBLE_CPP_UTILS_H_
