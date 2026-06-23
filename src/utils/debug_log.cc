#include "tensorflow/lite/micro/debug_log.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include "sl_iostream.h"
#include "sl_tflite_micro_debug_log.h"

static bool debug_log_enabled = true;

void sl_tflite_micro_enable_debug_log(bool enable)
{
  debug_log_enabled = enable;
}

bool sl_tflite_micro_is_debug_log_enabled(void)
{
  return debug_log_enabled;
}

#ifdef TF_LITE_STRIP_ERROR_STRINGS
extern "C" void DebugLog(const char* format, va_list args) { (void)format; (void)args; }
extern "C" int DebugVsnprintf(char* buffer, size_t buf_size, const char* format,
                              va_list vlist) {
  (void)buffer;
  (void)buf_size;
  (void)format;
  (void)vlist;
  return 0;
}
#else
extern "C" void DebugLog(const char* format, va_list args)
{
  if (!debug_log_enabled) {
    return;
  }
  constexpr size_t kMaxLogLen = 256;
  char log_buffer[kMaxLogLen];
  int len = vsnprintf(log_buffer, kMaxLogLen, format, args);
  if (len > 0) {
    size_t n = static_cast<size_t>(len) < kMaxLogLen ? static_cast<size_t>(len)
                                                     : kMaxLogLen - 1;
    sl_iostream_write(sl_iostream_get_default(), log_buffer, n);
  }
}

extern "C" int DebugVsnprintf(char* buffer, size_t buf_size, const char* format,
                              va_list vlist)
{
  return vsnprintf(buffer, buf_size, format, vlist);
}
#endif  // TF_LITE_STRIP_ERROR_STRINGS
