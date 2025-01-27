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
#if defined(CONFIG_BT_ENABLED) && defined(CONFIG_BT_NIMBLE_ROLE_CENTRAL)

# include "NimBLERemoteService.h"
# include "NimBLERemoteCharacteristic.h"
# include "NimBLERemoteGattUtils.h"
# include "NimBLEClient.h"
# include "NimBLEAttValue.h"
# include "NimBLEUtils.h"
# include "NimBLELog.h"

# include <climits>

static const char* LOG_TAG = "NimBLERemoteService";

/**
 * @brief Remote Service constructor.
 * @param [in] pClient A pointer to the client this belongs to.
 * @param [in] service A pointer to the structure with the service information.
 */
NimBLERemoteService::NimBLERemoteService(NimBLEClient* pClient, const ble_gatt_svc* service)
    : NimBLEAttribute{service->uuid, service->start_handle}, m_pClient{pClient}, m_endHandle{service->end_handle} {}

/**
 * @brief When deleting the service make sure we delete all characteristics and descriptors.
 */
NimBLERemoteService::~NimBLERemoteService() {
    deleteCharacteristics();
}

/**
 * @brief Get iterator to the beginning of the vector of remote characteristic pointers.
 * @return An iterator to the beginning of the vector of remote characteristic pointers.
 */
std::vector<NimBLERemoteCharacteristic*>::iterator NimBLERemoteService::begin() const {
    return m_vChars.begin();
}

/**
 * @brief Get iterator to the end of the vector of remote characteristic pointers.
 * @return An iterator to the end of the vector of remote characteristic pointers.
 */
std::vector<NimBLERemoteCharacteristic*>::iterator NimBLERemoteService::end() const {
    return m_vChars.end();
}

/**
 * @brief Get the remote characteristic object for the characteristic UUID.
 * @param [in] uuid Remote characteristic uuid.
 * @return A pointer to the remote characteristic object.
 */
NimBLERemoteCharacteristic* NimBLERemoteService::getCharacteristic(const char* uuid) const {
    return getCharacteristic(NimBLEUUID(uuid));
} // getCharacteristic

/**
 * @brief Get the characteristic object for the UUID.
 * @param [in] uuid Characteristic uuid.
 * @return A pointer to the characteristic object, or nullptr if not found.
 */
NimBLERemoteCharacteristic* NimBLERemoteService::getCharacteristic(const NimBLEUUID& uuid) const {
    NIMBLE_LOGD(LOG_TAG, ">> getCharacteristic: uuid: %s", uuid.toString().c_str());
    NimBLERemoteCharacteristic* pChar = nullptr;

    NimBLERemoteGattUtils::getAttr<NimBLERemoteCharacteristic>(uuid, &pChar, m_vChars,
      [this](const NimBLEUUID* u, NimBLERemoteCharacteristic** chr) {
        return retrieveCharacteristics(u, chr);
    });

    NIMBLE_LOGD(LOG_TAG, "<< getCharacteristic: %sfound", !pChar ? "not " : "");
    return pChar;
} // getCharacteristic

/**
 * @brief Get a pointer to the vector of found characteristics.
 * @param [in] refresh If true the current characteristics vector will cleared and
 * all characteristics for this service retrieved from the peripheral.
 * If false the vector will be returned with the currently stored characteristics of this service.
 * @return A read-only reference to the vector of characteristics retrieved for this service.
 */
const std::vector<NimBLERemoteCharacteristic*>& NimBLERemoteService::getCharacteristics(bool refresh) const {
    if (refresh) {
        deleteCharacteristics();
        retrieveCharacteristics();
    }

    return m_vChars;
} // getCharacteristics

/**
 * @brief Callback for Characteristic discovery.
 * @return success == 0 or error code.
 */
int NimBLERemoteService::chrDiscCB(uint16_t connHandle,
                                   const ble_gatt_error* error,
                                   const ble_gatt_chr*   chr,
                                   void*                 arg) {
    const int  rc        = error->status;
    auto       pTaskData = (NimBLETaskData*)arg;
    const auto pSvc      = (NimBLERemoteService*)pTaskData->m_pInstance;
    NIMBLE_LOGD(LOG_TAG, "Characteristic Discovery >> status: %d handle: %d", rc, (rc == 0) ? chr->def_handle : -1);

    // Make sure the discovery is for this device
    if (pSvc->getClient()->getConnHandle() != connHandle) {
        return 0;
    }

    if (rc == 0) {
        pSvc->m_vChars.push_back(new NimBLERemoteCharacteristic(pSvc, chr));
        return 0;
    }

    NimBLEUtils::taskRelease(*pTaskData, rc);
    NIMBLE_LOGD(LOG_TAG, "<< Characteristic Discovery%s", (rc == BLE_HS_ENOTCONN) ? "; Not connected" : "");
    return rc;
}

/**
 * @brief Retrieve all the characteristics for this service.
 * This function will not return until we have all the characteristics.
 * @return True if successfully retrieved, success = BLE_HS_EDONE.
 */
bool NimBLERemoteService::retrieveCharacteristics(const NimBLEUUID* uuid, NimBLERemoteCharacteristic** out) const {
    NIMBLE_LOGD(LOG_TAG, ">> retrieveCharacteristics() for service: %s", getUUID().toString().c_str());
    NimBLETaskData taskData(const_cast<NimBLERemoteService*>(this));
    const uint16_t hdlConn  = m_pClient->getConnHandle();
    const uint16_t hdlEnd   = getEndHandle();
    const uint16_t hdlStart = getHandle();
    // If this is the last handle then there are no more characteristics
    if (hdlStart == hdlEnd) {
        NIMBLE_LOGD(LOG_TAG, "<< retrieveCharacteristics(): found %d characteristics.", m_vChars.size());
        return true;
    }

    int rc = (uuid == nullptr)
           ? ble_gattc_disc_all_chrs(hdlConn, hdlStart, hdlEnd, chrDiscCB, &taskData)
           : ble_gattc_disc_chrs_by_uuid(hdlConn, hdlStart, hdlEnd, uuid->getBase(), chrDiscCB, &taskData);

    if (rc != 0) {
        NIMBLE_LOGE(LOG_TAG, "ble_gattc_disc_chrs rc=%d %s", rc, NimBLEUtils::returnCodeToString(rc));
        return false;
    }

    NimBLEUtils::taskWait(taskData, BLE_NPL_TIME_FOREVER);
    rc = taskData.m_flags;
    if (rc != BLE_HS_EDONE) {
        NIMBLE_LOGE(LOG_TAG, "<< retrieveCharacteristics(): failed: rc=%d %s", rc, NimBLEUtils::returnCodeToString(rc));
        return false;
    }

    *out = m_vChars.back();
    NIMBLE_LOGD(LOG_TAG, "<< retrieveCharacteristics(): found %d characteristics.", m_vChars.size());
    return true;
} // retrieveCharacteristics

/**
 * @brief Get the client associated with this service.
 * @return A reference to the client associated with this service.
 */
NimBLEClient* NimBLERemoteService::getClient() const {
    return m_pClient;
} // getClient

/**
 * @brief Read the value of a characteristic associated with this service.
 * @param [in] uuid The characteristic to read.
 * @returns a string containing the value or an empty string if not found or error.
 */
NimBLEAttValue NimBLERemoteService::getValue(const NimBLEUUID& uuid) const {
    const auto pChar = getCharacteristic(uuid);
    return pChar ? pChar->readValue()
                 : NimBLEAttValue{};
} // readValue

/**
 * @brief Set the value of a characteristic.
 * @param [in] uuid The characteristic UUID to set.
 * @param [in] value The value to set.
 * @returns true on success, false if not found or error
 */
bool NimBLERemoteService::setValue(const NimBLEUUID& uuid, const NimBLEAttValue& value) const {
    const auto pChar = getCharacteristic(uuid);
    return pChar ? pChar->writeValue(value)
                 : false;
} // setValue

/**
 * @brief Delete the characteristics in the characteristics vector.
 * @details We maintain a vector called m_characteristicsVector that contains pointers to BLERemoteCharacteristic
 * object references. Since we allocated these in this class, we are also responsible for deleting
 * them. This method does just that.
 */
void NimBLERemoteService::deleteCharacteristics() const {
    for (const auto& chr : m_vChars) {
        delete chr;
    }
    std::vector<NimBLERemoteCharacteristic*>{}.swap(m_vChars);
} // deleteCharacteristics

/**
 * @brief Delete characteristic by UUID
 * @param [in] uuid The UUID of the characteristic to be removed from the local database.
 * @return Number of characteristics left.
 */
size_t NimBLERemoteService::deleteCharacteristic(const NimBLEUUID& uuid) const {
    for (auto it = m_vChars.begin(); it != m_vChars.end(); ++it) {
        if ((*it)->getUUID() == uuid) {
            delete (*it);
            m_vChars.erase(it);
            break;
        }
    }

    return m_vChars.size();
} // deleteCharacteristic

/**
 * @brief Create a string representation of this remote service.
 * @return A string representation of this remote service.
 */
std::string NimBLERemoteService::toString() const {
    std::string res = "Service: uuid: " + m_uuid.toString() + ", start_handle: 0x";
    char        val[5];
    snprintf(val, sizeof(val), "%04x", getHandle());
    res += val;
    res += ", end_handle: 0x";
    snprintf(val, sizeof(val), "%04x", getEndHandle());
    res += val;

    for (const auto& chr : m_vChars) {
        res += "\n" + chr->toString();
    }

    return res;
} // toString

#endif /* CONFIG_BT_ENABLED && CONFIG_BT_NIMBLE_ROLE_CENTRAL */
