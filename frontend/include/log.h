
#ifndef _ONE_IDLC_LOG_H_
#define _ONE_IDLC_LOG_H_

#include <stdio.h>

void log_module_init();
/* log level in current file */
#define LOCAL_PRINT_LEVEL LOCAL_DEBUG

#define LOCAL_DEBUG     9
#define LOCAL_INFO      7
#define LOCAL_WARNING   5
#define LOCAL_ERROR     3
#define LOCAL_CRITICAL  1

/* Red     =>   CRITICAL and ERROR
 * Yellow  =>   WARNING
 * Green   =>   INFO
 * Blue    =>   DEBUG
 */
#define LOCAL_RED       "\033[31;1m"
#define LOCAL_YELLOW    "\033[0;33m"
#define LOCAL_GREEN     "\033[0;32m"
#define LOCAL_BULE      "\033[0;34m"
#define LOCAL_END       "\033[0m"


#define ALOGD(...) do { \
    SWITCH_PRINT(LOCAL_DEBUG, __VA_ARGS__); \
} while(0)

#define ALOGI(...) do { \
    SWITCH_PRINT(LOCAL_INFO, __VA_ARGS__); \
} while(0)

#define ALOGW(...) do { \
    SWITCH_PRINT(LOCAL_WARNING, __VA_ARGS__); \
} while(0)

#define ALOGE(...) do { \
    SWITCH_PRINT(LOCAL_ERROR, __VA_ARGS__); \
} while(0)

#define ALOGC(...) do { \
    SWITCH_PRINT(LOCAL_CRITICAL, __VA_ARGS__); \
} while(0)

#define SWITCH_PRINT(fmt, ...) do { \
    if (fmt <= LOCAL_PRINT_LEVEL) { \
        switch (fmt) { \
            case LOCAL_DEBUG: \
                printf(LOCAL_BULE); \
                break; \
            case LOCAL_INFO: \
                printf(LOCAL_GREEN); \
                break; \
            case LOCAL_WARNING: \
                printf(LOCAL_YELLOW); \
                break; \
            case LOCAL_ERROR: \
            case LOCAL_CRITICAL: \
                printf(LOCAL_RED); \
                break; \
            default: \
                break;\
        } \
        printf(__VA_ARGS__); \
        printf("\n"); \
        printf(LOCAL_END); \
    } \
} while(0)

#endif	//_ONE_IDLC_LOG_H_
