#ifdef __ANDROID__
#include <android/log.h>
#endif

#define LOG_TAG "QnnLlm"

#ifdef __ANDROID__
#define LOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, fmt, ##__VA_ARGS__)
#define LOGD(fmt, ...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, fmt, ##__VA_ARGS__)

#else
#define LOGI(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define LOGD(fmt, ...) printf(fmt, ##__VA_ARGS__)

#endif
