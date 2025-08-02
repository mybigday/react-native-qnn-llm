#ifdef __ANDROID__
#include <android/log.h>
#endif

#define LOG_TAG "QnnLlm"

#define LOG_INFO(fmt, ...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, fmt, ##__VA_ARGS__)

#else
#define LOG_INFO(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif
