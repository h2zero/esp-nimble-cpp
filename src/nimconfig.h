/** @file
 *
 * IGNORE THIS FILE IF USING ESP-IDF, USE MENUCONFIG TO SET NIMBLE OPTIONS.
 *
 * The config options here are for doxygen documentation only.
 */

#pragma once

#include "sdkconfig.h"
#include "nimconfig_rename.h"

#ifdef _DOXYGEN_

/** @brief Un-comment to change the number of simultaneous connections (esp controller max is 9) */
#define CONFIG_BT_NIMBLE_MAX_CONNECTIONS 3

/** @brief Un-comment to change the default MTU size */
#define CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU 255

/** @brief Un-comment to change default device name */
#define CONFIG_BT_NIMBLE_SVC_GAP_DEVICE_NAME "nimble"

/** @brief Un-comment to see debug log messages from the NimBLE host
 *  Uses approx. 32kB of flash memory.
 */
#define CONFIG_BT_NIMBLE_DEBUG

/** @brief Un-comment to see NimBLE host return codes as text debug log messages.
 *  Uses approx. 7kB of flash memory.
 */
#define CONFIG_NIMBLE_CPP_ENABLE_RETURN_CODE_TEXT

/** @brief Un-comment to see GAP event codes as text in debug log messages.
 *  Uses approx. 1kB of flash memory.
 */
#define CONFIG_NIMBLE_CPP_ENABLE_GAP_EVENT_CODE_TEXT

/** @brief Un-comment to see advertisment types as text while scanning in debug log messages.
 *  Uses approx. 250 bytes of flash memory.
 */
#define CONFIG_NIMBLE_CPP_ENABLE_ADVERTISMENT_TYPE_TEXT

/** @brief Un-comment to change the default GAP appearance */
#define CONFIG_BT_NIMBLE_SVC_GAP_APPEARANCE 0x0

 /** @brief Un-comment if not using NimBLE Client functions \n
 *  Reduces flash size by approx. 7kB.
 */
#define CONFIG_BT_NIMBLE_ROLE_CENTRAL_DISABLED

/** @brief Un-comment if not using NimBLE Scan functions \n
 *  Reduces flash size by approx. 26kB.
 */
#define CONFIG_BT_NIMBLE_ROLE_OBSERVER_DISABLED

/** @brief Un-comment if not using NimBLE Server functions \n
 *  Reduces flash size by approx. 16kB.
 */
#define CONFIG_BT_NIMBLE_ROLE_PERIPHERAL_DISABLED

/** @brief Un-comment if not using NimBLE Advertising functions \n
 *  Reduces flash size by approx. 5kB.
 */
#define CONFIG_BT_NIMBLE_ROLE_BROADCASTER_DISABLED

/** @brief Un-comment to change the number of devices allowed to store/bond with */
#define CONFIG_BT_NIMBLE_MAX_BONDS 3

/** @brief Un-comment to change the maximum number of CCCD subscriptions to store */
#define CONFIG_BT_NIMBLE_MAX_CCCDS 8

/** @brief Un-comment to change the random address refresh time (in seconds) */
#define CONFIG_BT_NIMBLE_RPA_TIMEOUT 900

/**
 * @brief Un-comment to change the number of MSYS buffers available.
 * @details MSYS is a system level mbuf registry. For prepare write & prepare \n
 * responses MBUFs are allocated out of msys_1 pool. This may need to be increased if\n
 * you are sending large blocks of data with a low MTU. E.g: 512 bytes with 23 MTU will fail.
 */
#define CONFIG_BT_NIMBLE_MSYS1_BLOCK_COUNT 12

/** @brief Un-comment to use external PSRAM for the NimBLE host */
#define CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL 1

/** @brief Un-comment to change the core NimBLE host runs on */
#define CONFIG_BT_NIMBLE_PINNED_TO_CORE 0

/** @brief Un-comment to change the stack size for the NimBLE host task */
#define CONFIG_BT_NIMBLE_TASK_STACK_SIZE 4096

#endif // _DOXYGEN_
