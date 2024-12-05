/*
 * NimBLELog.h
 *
 *  Created: on Feb 24 2020
 *      Author H2zero
 *
 */
#ifndef MAIN_NIMBLELOG_H_
#define MAIN_NIMBLELOG_H_

#include "nimconfig.h"

# if defined(CONFIG_BT_ENABLED)
#  if defined(CONFIG_NIMBLE_CPP_IDF) // using esp-idf
#   include "esp_log.h"
#   include "console/console.h"
#   ifndef CONFIG_NIMBLE_CPP_LOG_LEVEL
#    define CONFIG_NIMBLE_CPP_LOG_LEVEL 0
#   endif

#   if defined(CONFIG_NIMBLE_LOG_OVERRIDE)

#    define NIMBLE_GET_LOG_COLOR(letter)

#    if CONFIG_LOG_COLORS
#     if defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_ERR_BLACK)
#      define NIMBLE_LOG_COLOR_E     LOG_COLOR(LOG_COLOR_BLACK)
#     elif defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_ERR_RED)
#      define NIMBLE_LOG_COLOR_E     LOG_COLOR(LOG_COLOR_RED)
#     elif defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_ERR_GREEN)
#      define NIMBLE_LOG_COLOR_E     LOG_COLOR(LOG_COLOR_GREEN)
#     elif defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_ERR_YELLOW)
#      define NIMBLE_LOG_COLOR_E     LOG_COLOR(LOG_COLOR_BROWN)
#     elif defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_ERR_BLUE)
#      define NIMBLE_LOG_COLOR_E     LOG_COLOR(LOG_COLOR_BLUE)
#     elif defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_ERR_PURPLE)
#      define NIMBLE_LOG_COLOR_E     LOG_COLOR(LOG_COLOR_PURPLE)
#     elif defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_ERR_CYAN)
#      define NIMBLE_LOG_COLOR_E     LOG_COLOR(LOG_COLOR_CYAN)
#     else
#      define NIMBLE_LOG_COLOR_E
#     endif

#     if defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_WARN_BLACK)
#      define NIMBLE_LOG_COLOR_W     LOG_COLOR(LOG_COLOR_BLACK)
#     elif defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_WARN_RED)
#      define NIMBLE_LOG_COLOR_W     LOG_COLOR(LOG_COLOR_RED)
#     elif defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_WARN_GREEN)
#      define NIMBLE_LOG_COLOR_W     LOG_COLOR(LOG_COLOR_GREEN)
#     elif defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_WARN_YELLOW)
#      define NIMBLE_LOG_COLOR_W     LOG_COLOR(LOG_COLOR_BROWN)
#     elif defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_WARN_BLUE)
#      define NIMBLE_LOG_COLOR_W     LOG_COLOR(LOG_COLOR_BLUE)
#     elif defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_WARN_PURPLE)
#      define NIMBLE_LOG_COLOR_W     LOG_COLOR(LOG_COLOR_PURPLE)
#     elif defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_WARN_CYAN)
#      define NIMBLE_LOG_COLOR_W     LOG_COLOR(LOG_COLOR_CYAN)
#     else
#      define NIMBLE_LOG_COLOR_W
#     endif

#     if defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_INFO_BLACK)
#      define NIMBLE_LOG_COLOR_I     LOG_COLOR(LOG_COLOR_BLACK)
#     elif defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_INFO_RED)
#      define NIMBLE_LOG_COLOR_I     LOG_COLOR(LOG_COLOR_RED)
#     elif defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_INFO_GREEN)
#      define NIMBLE_LOG_COLOR_I     LOG_COLOR(LOG_COLOR_GREEN)
#     elif defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_INFO_YELLOW)
#      define NIMBLE_LOG_COLOR_I     LOG_COLOR(LOG_COLOR_BROWN)
#     elif defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_INFO_BLUE)
#      define NIMBLE_LOG_COLOR_I     LOG_COLOR(LOG_COLOR_BLUE)
#     elif defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_INFO_PURPLE)
#      define NIMBLE_LOG_COLOR_I     LOG_COLOR(LOG_COLOR_PURPLE)
#     elif defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_INFO_CYAN)
#      define NIMBLE_LOG_COLOR_I     LOG_COLOR(LOG_COLOR_CYAN)
#     else
#      define NIMBLE_LOG_COLOR_I
#     endif

#     if defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_DEBUG_BLACK)
#      define NIMBLE_LOG_COLOR_D     LOG_COLOR(LOG_COLOR_BLACK)
#     elif defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_DEBUG_RED)
#      define NIMBLE_LOG_COLOR_D     LOG_COLOR(LOG_COLOR_RED)
#     elif defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_DEBUG_GREEN)
#      define NIMBLE_LOG_COLOR_D     LOG_COLOR(LOG_COLOR_GREEN)
#     elif defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_DEBUG_YELLOW)
#      define NIMBLE_LOG_COLOR_D     LOG_COLOR(LOG_COLOR_BROWN)
#     elif defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_DEBUG_BLUE)
#      define NIMBLE_LOG_COLOR_D     LOG_COLOR(LOG_COLOR_BLUE)
#     elif defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_DEBUG_PURPLE)
#      define NIMBLE_LOG_COLOR_D     LOG_COLOR(LOG_COLOR_PURPLE)
#     elif defined(CONFIG_NIMBLE_LOG_OVERRIDE_COLOR_DEBUG_CYAN)
#      define NIMBLE_LOG_COLOR_D     LOG_COLOR(LOG_COLOR_CYAN)
#     else
#      define NIMBLE_LOG_COLOR_D
#     endif
#    else //CONFIG_LOG_COLORS
#     define NIMBLE_LOG_COLOR_E
#     define NIMBLE_LOG_COLOR_W
#     define NIMBLE_LOG_COLOR_I
#     define NIMBLE_LOG_COLOR_D
#    endif //CONFIG_LOG_COLORS

#    define NIMBLE_LOG_FORMAT(letter, tag, fmt) \
        NIMBLE_LOG_COLOR_##letter #letter " (%lu) %s: " fmt LOG_RESET_COLOR "\r\n", esp_log_timestamp(), tag

#    if CONFIG_NIMBLE_CPP_LOG_LEVEL >= 4
#     define NIMBLE_LOGD( tag, format, ... ) console_printf(NIMBLE_LOG_FORMAT(D, tag, format), ##__VA_ARGS__)
#    else
#     define NIMBLE_LOGD( tag, format, ... )
#    endif

#    if CONFIG_NIMBLE_CPP_LOG_LEVEL >= 3
#     define NIMBLE_LOGI( tag, format, ... ) console_printf(NIMBLE_LOG_FORMAT(I, tag, format), ##__VA_ARGS__)
#    else
#     define NIMBLE_LOGI( tag, format, ... )
#    endif

#    if CONFIG_NIMBLE_CPP_LOG_LEVEL >= 2
#     define NIMBLE_LOGW( tag, format, ... ) console_printf(NIMBLE_LOG_FORMAT(W, tag, format), ##__VA_ARGS__)
#    else
#     define NIMBLE_LOGW( tag, format, ... )
#    endif

#    if CONFIG_NIMBLE_CPP_LOG_LEVEL >= 1
#     define NIMBLE_LOGE( tag, format, ... ) console_printf(NIMBLE_LOG_FORMAT(E, tag, format), ##__VA_ARGS__)
#    else
#     define NIMBLE_LOGE( tag, format, ... )
#    endif


#   else
#    define NIMBLE_CPP_LOG_PRINT(level, tag, format, ...) do { \
    if (CONFIG_NIMBLE_CPP_LOG_LEVEL >= level) \
      ESP_LOG_LEVEL_LOCAL(level, tag, format, ##__VA_ARGS__); \
    } while(0)

#    define NIMBLE_LOGD(tag, format, ...) \
      NIMBLE_CPP_LOG_PRINT(ESP_LOG_DEBUG, tag, format, ##__VA_ARGS__)

#    define NIMBLE_LOGI(tag, format, ...) \
      NIMBLE_CPP_LOG_PRINT(ESP_LOG_INFO, tag, format, ##__VA_ARGS__)

#    define NIMBLE_LOGW(tag, format, ...) \
      NIMBLE_CPP_LOG_PRINT(ESP_LOG_WARN, tag, format, ##__VA_ARGS__)

#    define NIMBLE_LOGE(tag, format, ...) \
      NIMBLE_CPP_LOG_PRINT(ESP_LOG_ERROR, tag, format, ##__VA_ARGS__)
#   endif

#  else // using Arduino
#   include "nimble/porting/nimble/include/syscfg/syscfg.h"
#   include "nimble/console/console.h"
#   ifndef CONFIG_NIMBLE_CPP_LOG_LEVEL
#     if defined(ARDUINO_ARCH_ESP32) && defined(CORE_DEBUG_LEVEL)
#       define CONFIG_NIMBLE_CPP_LOG_LEVEL CORE_DEBUG_LEVEL
#     else
#       define CONFIG_NIMBLE_CPP_LOG_LEVEL 0
#     endif
#   endif

#   if CONFIG_NIMBLE_CPP_LOG_LEVEL >= 4
#     define NIMBLE_LOGD( tag, format, ... ) console_printf("D %s: " format "\n", tag, ##__VA_ARGS__)
#   else
#     define NIMBLE_LOGD( tag, format, ... ) (void)tag
#   endif

#   if CONFIG_NIMBLE_CPP_LOG_LEVEL >= 3
#     define NIMBLE_LOGI( tag, format, ... ) console_printf("I %s: " format "\n", tag, ##__VA_ARGS__)
#   else
#     define NIMBLE_LOGI( tag, format, ... ) (void)tag
#   endif

#   if CONFIG_NIMBLE_CPP_LOG_LEVEL >= 2
#     define NIMBLE_LOGW( tag, format, ... ) console_printf("W %s: " format "\n", tag, ##__VA_ARGS__)
#   else
#     define NIMBLE_LOGW( tag, format, ... ) (void)tag
#   endif

#   if CONFIG_NIMBLE_CPP_LOG_LEVEL >= 1
#     define NIMBLE_LOGE( tag, format, ... ) console_printf("E %s: " format "\n", tag, ##__VA_ARGS__)
#   else
#     define NIMBLE_LOGE( tag, format, ... ) (void)tag
#   endif

#  endif /* CONFIG_NIMBLE_CPP_IDF */

#  define NIMBLE_LOGC( tag, format, ... ) console_printf("CRIT %s: " format "\n", tag, ##__VA_ARGS__)

# endif /* CONFIG_BT_ENABLED */
#endif /* MAIN_NIMBLELOG_H_ */
