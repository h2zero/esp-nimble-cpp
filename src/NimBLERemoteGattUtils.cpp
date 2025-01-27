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
 * @param [in] attr Pointer to hold result.
 * @param [in] vec Vector to search through before trying to get attribute.
 * @param [in] getter Attribute getter function to call.
 */
template <typename T>
void NimBLERemoteGattUtils::getAttr(const NimBLEUUID& uuid, T** attr, const std::vector<T*>& vec, const std::function<bool(const NimBLEUUID*, T**)>& getter) {
    // Check if already exists.
    for (const auto& v : vec) {
        if (v->getUUID() == uuid) {
            *attr = v;
            return;
        }
    }
    // Exit if request failed or uuid was found.
    if (!getter(&uuid, attr) || *attr) {
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

using svc = NimBLERemoteService; using chr = NimBLERemoteCharacteristic; using dsc = NimBLERemoteDescriptor;
template void NimBLERemoteGattUtils::getAttr<svc>(const NimBLEUUID&, svc**, const std::vector<svc*>&, const std::function<bool(const NimBLEUUID*, svc**)>&);
template void NimBLERemoteGattUtils::getAttr<chr>(const NimBLEUUID&, chr**, const std::vector<chr*>&, const std::function<bool(const NimBLEUUID*, chr**)>&);
template void NimBLERemoteGattUtils::getAttr<dsc>(const NimBLEUUID&, dsc**, const std::vector<dsc*>&, const std::function<bool(const NimBLEUUID*, dsc**)>&);

#endif // CONFIG_BT_ENABLED
