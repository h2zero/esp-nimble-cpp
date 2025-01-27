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

#ifndef NIMBLE_CPP_GATT_UTILS_H_
#define NIMBLE_CPP_GATT_UTILS_H_

#include "nimconfig.h"
#if defined(CONFIG_BT_ENABLED)
# include "NimBLEUUID.h"
# include <functional>
# include <vector>
# include <string>

/**
 * @brief A BLE Remote GATT Utility class for getting an attribute.
 */
class NimBLERemoteGattUtils {
  public:
    template <typename T>
    static void getAttr(const NimBLEUUID& uuid, T** attr, const std::vector<T*>& vec,
                        const std::function<bool(const NimBLEUUID*, T**)>& getter);
}; // NimBLERemoteGattUtils

#endif /* CONFIG_BT_ENABLED */
#endif /* NIMBLE_CPP_GATT_UTILS_H_ */
