/*
 * NimBLEScan.cpp
 *
 *  Created: on Jan 24 2020
 *      Author H2zero
 *
 * Originally:
 *
 * BLEScan.cpp
 *
 *  Created on: Jul 1, 2017
 *      Author: kolban
 */

#include "nimconfig.h"
#if defined(CONFIG_BT_ENABLED) && defined(CONFIG_BT_NIMBLE_ROLE_OBSERVER)

# include "NimBLEScan.h"
# include "NimBLEDevice.h"
# include "NimBLELog.h"

# include <string>
# include <climits>

static const char* LOG_TAG = "NimBLEScan";

/**
 * @brief Scan constructor.
 */
NimBLEScan::NimBLEScan()
    : m_pScanCallbacks{nullptr},
      m_scanParams{
          .itvl{0}, .window{0}, .filter_policy{BLE_HCI_SCAN_FILT_NO_WL}, .limited{0}, .passive{1}, .filter_duplicates{1}},
      m_duration{BLE_HS_FOREVER},
      m_pTaskData{nullptr},
      m_maxResults{0xFF} {}

/**
 * @brief Scan destructor, release any allocated resources.
 */
NimBLEScan::~NimBLEScan() {
    clearResults();
}

/**
 * @brief Handle GAP events related to scans.
 * @param [in] event The event type for this event.
 * @param [in] param Parameter data for this event.
 */
int NimBLEScan::handleGapEvent(ble_gap_event* event, void* arg) {
    (void)arg;
    NimBLEScan* pScan = NimBLEDevice::getScan();

    switch (event->type) {
        case BLE_GAP_EVENT_EXT_DISC:
        case BLE_GAP_EVENT_DISC: {
# if CONFIG_BT_NIMBLE_EXT_ADV
            const auto& disc        = event->ext_disc;
            const bool  isLegacyAdv = disc.props & BLE_HCI_ADV_LEGACY_MASK;
            const auto  event_type  = isLegacyAdv ? disc.legacy_event_type : disc.props;
# else
            const auto& disc        = event->disc;
            const bool  isLegacyAdv = true;
            const auto  event_type  = disc.event_type;
# endif
            NimBLEAddress advertisedAddress(disc.addr);

            // stop processing if we don't want to see it or are already connected
            if (NimBLEDevice::isIgnored(advertisedAddress)) {
                NIMBLE_LOGI(LOG_TAG, "Ignoring device: address: %s", advertisedAddress.toString().c_str());
                return 0;
            }

            NimBLEAdvertisedDevice* advertisedDevice = nullptr;

            // If we've seen this device before get a pointer to it from the vector
            for (auto& ad : pScan->m_scanResults.m_deviceVec) {
# if CONFIG_BT_NIMBLE_EXT_ADV
                // Same address but different set ID should create a new advertised device.
                if (ad->getAddress() == advertisedAddress && ad->getSetId() == disc.sid) {
# else
                if (ad->getAddress() == advertisedAddress) {
# endif
                    advertisedDevice = ad;
                    break;
                }
            }

            // If we haven't seen this device before; create a new instance and insert it in the vector.
            // Otherwise just update the relevant parameters of the already known device.
            if (advertisedDevice == nullptr) {
                if (event_type == BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP) {
                    NIMBLE_LOGW(LOG_TAG, "Scan response without advertisement: %s", advertisedAddress.toString().c_str());
                }

                // Check if we have reach the scan results limit, ignore this one if so.
                // We still need to store each device when maxResults is 0 to be able to append the scan results
                if (pScan->m_maxResults > 0 && pScan->m_maxResults < 0xFF &&
                    (pScan->m_scanResults.m_deviceVec.size() >= pScan->m_maxResults)) {
                    return 0;
                }

                advertisedDevice = new NimBLEAdvertisedDevice(event, event_type);
                pScan->m_scanResults.m_deviceVec.push_back(advertisedDevice);
                NIMBLE_LOGI(LOG_TAG, "New advertiser: %s", advertisedAddress.toString().c_str());
            } else if (advertisedDevice != nullptr) {
                advertisedDevice->update(event, event_type);
                NIMBLE_LOGI(LOG_TAG, "Updated advertiser: %s", advertisedAddress.toString().c_str());
            } else {
                // unknown device
                return 0;
            }

            if (pScan->m_pScanCallbacks) {
                if (advertisedDevice->m_callbackSent == 0 || !pScan->m_scanParams.filter_duplicates) {
                    advertisedDevice->m_callbackSent = 1;
                    pScan->m_pScanCallbacks->onDiscovered(advertisedDevice);
                }

                if (pScan->m_scanParams.filter_duplicates && advertisedDevice->m_callbackSent >= 2) {
                    return 0;
                }

                // If not active scanning or scan response is not available
                // or extended advertisement scanning, report the result to the callback now.
                if (pScan->m_scanParams.passive || !isLegacyAdv ||
                    (advertisedDevice->getAdvType() != BLE_HCI_ADV_TYPE_ADV_IND &&
                     advertisedDevice->getAdvType() != BLE_HCI_ADV_TYPE_ADV_SCAN_IND)) {
                    advertisedDevice->m_callbackSent = 2;
                    pScan->m_pScanCallbacks->onResult(advertisedDevice);

                    // Otherwise, wait for the scan response so we can report the complete data.
                } else if (isLegacyAdv && event_type == BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP) {
                    advertisedDevice->m_callbackSent = 2;
                    pScan->m_pScanCallbacks->onResult(advertisedDevice);
                }
                // If not storing results and we have invoked the callback, delete the device.
                if (pScan->m_maxResults == 0 && advertisedDevice->m_callbackSent >= 2) {
                    pScan->erase(advertisedAddress);
                }
            }

            return 0;
        }

        case BLE_GAP_EVENT_DISC_COMPLETE: {
            NIMBLE_LOGD(LOG_TAG, "discovery complete; reason=%d", event->disc_complete.reason);

            if (pScan->m_maxResults == 0) {
                pScan->clearResults();
            }

            if (pScan->m_pScanCallbacks != nullptr) {
                pScan->m_pScanCallbacks->onScanEnd(pScan->m_scanResults);
            }

            if (pScan->m_pTaskData != nullptr) {
                NimBLEUtils::taskRelease(*pScan->m_pTaskData, event->disc_complete.reason);
            }

            return 0;
        }

        default:
            return 0;
    }
} // handleGapEvent

/**
 * @brief Should we perform an active or passive scan?
 * The default is a passive scan. An active scan means that we will request a scan response.
 * @param [in] active If true, we perform an active scan otherwise a passive scan.
 */
void NimBLEScan::setActiveScan(bool active) {
    m_scanParams.passive = !active;
} // setActiveScan

/**
 * @brief Set whether or not the BLE controller should only report results
 * from devices it has not already seen.
 * @param [in] enabled If true, scanned devices will only be reported once.
 * @details The controller has a limited buffer and will start reporting
 * duplicate devices once the limit is reached.
 */
void NimBLEScan::setDuplicateFilter(bool enabled) {
    m_scanParams.filter_duplicates = enabled;
} // setDuplicateFilter

/**
 * @brief Set whether or not the BLE controller only report scan results
 * from devices advertising in limited discovery mode, i.e. directed advertising.
 * @param [in] enabled If true, only limited discovery devices will be in scan results.
 */
void NimBLEScan::setLimitedOnly(bool enabled) {
    m_scanParams.limited = enabled;
} // setLimited

/**
 * @brief Sets the scan filter policy.
 * @param [in] filter Can be one of:
 * * BLE_HCI_SCAN_FILT_NO_WL             (0)
 *      Scanner processes all advertising packets (white list not used) except\n
 *      directed, connectable advertising packets not sent to the scanner.
 * * BLE_HCI_SCAN_FILT_USE_WL            (1)
 *      Scanner processes advertisements from white list only. A connectable,\n
 *      directed advertisement is ignored unless it contains scanners address.
 * * BLE_HCI_SCAN_FILT_NO_WL_INITA       (2)
 *      Scanner process all advertising packets (white list not used). A\n
 *      connectable, directed advertisement shall not be ignored if the InitA
 *      is a resolvable private address.
 * * BLE_HCI_SCAN_FILT_USE_WL_INITA      (3)
 *      Scanner process advertisements from white list only. A connectable,\n
 *      directed advertisement shall not be ignored if the InitA is a
 *      resolvable private address.
 */
void NimBLEScan::setFilterPolicy(uint8_t filter) {
    m_scanParams.filter_policy = filter;
} // setFilterPolicy

/**
 * @brief Sets the max number of results to store.
 * @param [in] maxResults The number of results to limit storage to\n
 * 0 == none (callbacks only) 0xFF == unlimited, any other value is the limit.
 */
void NimBLEScan::setMaxResults(uint8_t maxResults) {
    m_maxResults = maxResults;
}

/**
 * @brief Set the call backs to be invoked.
 * @param [in] pScanCallbacks Call backs to be invoked.
 * @param [in] wantDuplicates  True if we wish to be called back with duplicates.  Default is false.
 */
void NimBLEScan::setScanCallbacks(NimBLEScanCallbacks* pScanCallbacks, bool wantDuplicates) {
    setDuplicateFilter(!wantDuplicates);
    m_pScanCallbacks = pScanCallbacks;
} // setScanCallbacks

/**
 * @brief Set the interval to scan.
 * @param [in] intervalMSecs The scan interval (how often) in milliseconds.
 */
void NimBLEScan::setInterval(uint16_t intervalMSecs) {
    m_scanParams.itvl = intervalMSecs / 0.625;
} // setInterval

/**
 * @brief Set the window to actively scan.
 * @param [in] windowMSecs How long to actively scan.
 */
void NimBLEScan::setWindow(uint16_t windowMSecs) {
    m_scanParams.window = windowMSecs / 0.625;
} // setWindow

/**
 * @brief Get the status of the scanner.
 * @return true if scanning or scan starting.
 */
bool NimBLEScan::isScanning() {
    return ble_gap_disc_active();
}

/**
 * @brief Start scanning.
 * @param [in] duration The duration in milliseconds for which to scan. 0 == scan forever.
 * @param [in] is_continue Set to true to save previous scan results, false to clear them.
 * @param [in] restart Set to true to restart the scan if already in progress.
 *             this is useful to clear the duplicate filter so all devices are reported again.
 * @return True if scan started or false if there was an error.
 */
bool NimBLEScan::start(uint32_t duration, bool is_continue, bool restart) {
    NIMBLE_LOGD(LOG_TAG, ">> start: duration=%" PRIu32, duration);

    if (ble_gap_conn_active()) {
        NIMBLE_LOGE(LOG_TAG, "Connection in progress, cannot start scan");
        return false;
    }

    if (isScanning()) {
        if(restart) {
            //NIMBLE_LOGI(LOG_TAG, "Scan already in progress, stopping it");
            if (!stop()) {
                return false;
            }
        } else {
            NIMBLE_LOGI(LOG_TAG, "Scan already in progress");
            return true;
        }
    }

    if (!is_continue) {
        clearResults();
    }

    // Save the duration in the case that the host is reset so we can reuse it.
    m_duration = duration;

    // If 0 duration specified then we assume a continuous scan is desired.
    if (duration == 0) {
        duration = BLE_HS_FOREVER;
    }

# if CONFIG_BT_NIMBLE_EXT_ADV
    ble_gap_ext_disc_params scan_params;
    scan_params.passive = m_scanParams.passive;
    scan_params.itvl    = m_scanParams.itvl;
    scan_params.window  = m_scanParams.window;
    int rc              = ble_gap_ext_disc(NimBLEDevice::m_ownAddrType,
                              duration / 10,
                              0,
                              m_scanParams.filter_duplicates,
                              m_scanParams.filter_policy,
                              m_scanParams.limited,
                              &scan_params,
                              &scan_params,
                              NimBLEScan::handleGapEvent,
                              NULL);
# else
    int rc = ble_gap_disc(NimBLEDevice::m_ownAddrType, duration, &m_scanParams, NimBLEScan::handleGapEvent, NULL);
# endif
    switch (rc) {
        case 0:
        case BLE_HS_EALREADY:
            NIMBLE_LOGD(LOG_TAG, "Scan started");
            break;

        case BLE_HS_EBUSY:
            NIMBLE_LOGE(LOG_TAG, "Unable to scan - connection in progress.");
            break;

        case BLE_HS_ETIMEOUT_HCI:
        case BLE_HS_EOS:
        case BLE_HS_ECONTROLLER:
        case BLE_HS_ENOTSYNCED:
            NIMBLE_LOGE(LOG_TAG, "Unable to scan - Host Reset");
            break;

        default:
            NIMBLE_LOGE(LOG_TAG, "Error starting scan; rc=%d, %s", rc, NimBLEUtils::returnCodeToString(rc));
            break;
    }

    NIMBLE_LOGD(LOG_TAG, "<< start()");
    return rc == 0 || rc == BLE_HS_EALREADY;
} // start

/**
 * @brief Stop an in progress scan.
 * @return True if successful.
 */
bool NimBLEScan::stop() {
    NIMBLE_LOGD(LOG_TAG, ">> stop()");

    int rc = ble_gap_disc_cancel();
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        NIMBLE_LOGE(LOG_TAG, "Failed to cancel scan; rc=%d", rc);
        return false;
    }

    if (m_maxResults == 0) {
        clearResults();
    }

    if (m_pTaskData != nullptr) {
        NimBLEUtils::taskRelease(*m_pTaskData);
    }

    NIMBLE_LOGD(LOG_TAG, "<< stop()");
    return true;
} // stop

/**
 * @brief Delete peer device from the scan results vector.
 * @param [in] address The address of the device to delete from the results.
 * @details After disconnecting, it may be required in the case we were connected to a device without a public address.
 */
void NimBLEScan::erase(const NimBLEAddress& address) {
    NIMBLE_LOGD(LOG_TAG, "erase device: %s", address.toString().c_str());
    for (auto it = m_scanResults.m_deviceVec.begin(); it != m_scanResults.m_deviceVec.end(); ++it) {
        if ((*it)->getAddress() == address) {
            delete *it;
            m_scanResults.m_deviceVec.erase(it);
            break;
        }
    }
}

/**
 * @brief If the host reset and re-synced this is called.
 * If the application was scanning indefinitely with a callback, restart it.
 */
void NimBLEScan::onHostSync() {
    if (m_duration == 0 && m_pScanCallbacks != nullptr) {
        start(0, false);
    }
}

/**
 * @brief Start scanning and block until scanning has been completed.
 * @param [in] duration The duration in milliseconds for which to scan.
 * @param [in] is_continue Set to true to save previous scan results, false to clear them.
 * @return The scan results.
 */
NimBLEScanResults NimBLEScan::getResults(uint32_t duration, bool is_continue) {
    if (duration == 0) {
        NIMBLE_LOGW(LOG_TAG, "Blocking scan called with duration = forever");
    }

    if (m_pTaskData != nullptr) {
        NIMBLE_LOGE(LOG_TAG, "Scan already in progress");
        return m_scanResults;
    }

    NimBLETaskData taskData;
    m_pTaskData = &taskData;

    if (start(duration, is_continue)) {
        NimBLEUtils::taskWait(taskData, BLE_NPL_TIME_FOREVER);
    }

    m_pTaskData = nullptr;
    return m_scanResults;
} // getResults

/**
 * @brief Get the results of the scan.
 * @return NimBLEScanResults object.
 */
NimBLEScanResults NimBLEScan::getResults() {
    return m_scanResults;
}

/**
 * @brief Clear the results of the scan.
 */
void NimBLEScan::clearResults() {
    for (auto& it : m_scanResults.m_deviceVec) {
        delete it;
    }
    std::vector<NimBLEAdvertisedDevice*>().swap(m_scanResults.m_deviceVec);
} // clearResults

/**
 * @brief Dump the scan results to the log.
 */
void NimBLEScanResults::dump() {
    NIMBLE_LOGD(LOG_TAG, ">> Dump scan results:");
    for (int i = 0; i < getCount(); i++) {
        NIMBLE_LOGI(LOG_TAG, "- %s", getDevice(i).toString().c_str());
    }
} // dump

/**
 * @brief Get the count of devices found in the last scan.
 * @return The number of devices found in the last scan.
 */
int NimBLEScanResults::getCount() {
    return m_deviceVec.size();
} // getCount

/**
 * @brief Return the specified device at the given index.
 * The index should be between 0 and getCount()-1.
 * @param [in] i The index of the device.
 * @return The device at the specified index.
 */
NimBLEAdvertisedDevice NimBLEScanResults::getDevice(uint32_t i) {
    return *m_deviceVec[i];
}

/**
 * @brief Get iterator to the beginning of the vector of advertised device pointers.
 * @return An iterator to the beginning of the vector of advertised device pointers.
 */
std::vector<NimBLEAdvertisedDevice*>::iterator NimBLEScanResults::begin() {
    return m_deviceVec.begin();
}

/**
 * @brief Get iterator to the end of the vector of advertised device pointers.
 * @return An iterator to the end of the vector of advertised device pointers.
 */
std::vector<NimBLEAdvertisedDevice*>::iterator NimBLEScanResults::end() {
    return m_deviceVec.end();
}

/**
 * @brief Get a pointer to the specified device at the given address.
 * If the address is not found a nullptr is returned.
 * @param [in] address The address of the device.
 * @return A pointer to the device at the specified address.
 */
NimBLEAdvertisedDevice* NimBLEScanResults::getDevice(const NimBLEAddress& address) {
    for (size_t index = 0; index < m_deviceVec.size(); index++) {
        if (m_deviceVec[index]->getAddress() == address) {
            return m_deviceVec[index];
        }
    }

    return nullptr;
}

#endif /* CONFIG_BT_ENABLED && CONFIG_BT_NIMBLE_ROLE_OBSERVER */
