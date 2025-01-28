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

#include "nimconfig.h"
#if defined(CONFIG_BT_ENABLED)

# include "NimBLERemoteGattUtils.h"
# include "NimBLEAddress.h"
# include "NimBLERemoteService.h"
# include "NimBLERemoteDescriptor.h"
# include "NimBLERemoteCharacteristic.h"

/**
 * @brief Get an attribute matching a uuid.
 * @param [in] uuid Search for this uuid.
 * @param [in] vec Vector to search through before trying to get attribute.
 * @param [in] getter Attribute getter function to call.
 */
template <typename T>
T* NimBLERemoteGattUtils::getAttr(const NimBLEUUID& uuid, const std::vector<T*>& vec, const std::function<bool(const NimBLEUUID*)>& getter) {
    // Check if already exists.
    for (const auto& v : vec) {
        if (v->getUUID() == uuid) {
            return v;
        }
    }
    // Exit if request failed or uuid was found.
    if (!getter(&uuid)) {
        return nullptr;
    }
    // Try again with 128 bit uuid if request succeeded with no uuid found.
    if (uuid.bitSize() == BLE_UUID_TYPE_16 || uuid.bitSize() == BLE_UUID_TYPE_32) {
        NimBLEUUID uuid128 = NimBLEUUID(uuid).to128();
        bool found = getter(&uuid128);
        return found ? vec.back() : nullptr;
    }
    // Try again with 16 bit uuid if request succeeded with no uuid found.
    // If the uuid was 128 bit but not of the BLE base type this check will fail.
    NimBLEUUID uuid16 = NimBLEUUID(uuid).to16();
    if (uuid16.bitSize() == BLE_UUID_TYPE_16) {
        bool found = getter(&uuid16);
        return found ? vec.back() : nullptr;
    }

    return nullptr;
}

using svc = NimBLERemoteService; using chr = NimBLERemoteCharacteristic; using dsc = NimBLERemoteDescriptor;
template svc* NimBLERemoteGattUtils::getAttr<svc>(const NimBLEUUID&, const std::vector<svc*>&, const std::function<bool(const NimBLEUUID*)>&);
template chr* NimBLERemoteGattUtils::getAttr<chr>(const NimBLEUUID&, const std::vector<chr*>&, const std::function<bool(const NimBLEUUID*)>&);
template dsc* NimBLERemoteGattUtils::getAttr<dsc>(const NimBLEUUID&, const std::vector<dsc*>&, const std::function<bool(const NimBLEUUID*)>&);

#endif // CONFIG_BT_ENABLED
