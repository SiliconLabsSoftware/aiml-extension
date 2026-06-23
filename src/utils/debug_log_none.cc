#include "tensorflow/lite/micro/debug_log.h"
#include <cstdarg>
#include <cstddef>
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

extern "C" void DebugLog(const char* format, va_list args)
{
  // Discard all debug log messages
  (void)format;
  (void)args;
}

extern "C" int DebugVsnprintf(char* buffer, size_t buf_size, const char* format,
                              va_list vlist)
{
  (void)buffer;
  (void)buf_size;
  (void)format;
  (void)vlist;
  return 0;
}
