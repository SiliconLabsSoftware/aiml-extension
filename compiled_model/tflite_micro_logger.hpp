/*******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#include "tflite_micro_model_config.h"
#pragma once

#include "tensorflow/lite/micro/micro_log.h"
#include "tflite_micro_model_helper.hpp"



#ifndef TFLITE_MICRO_LOG_LEVEL
#define TFLITE_MICRO_LOG_LEVEL 1 // Default log level is INFO
#endif

/**
 * @addtogroup tflite_micro_logger
 * @{
 */
#if !defined(NPU_TOOLKIT_DEBUG) && (TFLITE_MICRO_LOG_LEVEL <= 0)
  /**
   * If TFLITE_MICRO_LOG_LEVEL <= 0 then debug log are enabled
   */
  #define NPU_TOOLKIT_DEBUG(msg, ...) MicroPrintf(msg, ## __VA_ARGS__)
#endif // TFLITE_MICRO_LOG_LEVEL <= 0

#if !defined(NPU_TOOLKIT_INFO) && (TFLITE_MICRO_LOG_LEVEL <= 1)
  /**
   * If TFLITE_MICRO_LOG_LEVEL <= 1 then info log are enabled
   */
  #define NPU_TOOLKIT_INFO(msg, ...) MicroPrintf(msg, ## __VA_ARGS__)
#endif // TFLITE_MICRO_LOG_LEVEL <= 1

#if !defined(NPU_TOOLKIT_WARN) && (TFLITE_MICRO_LOG_LEVEL <= 2)
  /**
   * If TFLITE_MICRO_LOG_LEVEL <= 2 then warning log are enabled
   */
  #define NPU_TOOLKIT_WARN(msg, ...) MicroPrintf(msg, ## __VA_ARGS__)
#endif // TFLITE_MICRO_LOG_LEVEL <= 2

#if !defined(NPU_TOOLKIT_ERROR) && (TFLITE_MICRO_LOG_LEVEL <= 3)
  /**
   * If TFLITE_MICRO_LOG_LEVEL <= 3 then error log are enabled
   */
  #define NPU_TOOLKIT_ERROR(msg, ...) MicroPrintf(msg, ## __VA_ARGS__)
#endif // TFLITE_MICRO_LOG_LEVEL <= 3
/**
 * @}
 */






/**
 * @addtogroup tflite_micro_logger
 * @{
 */


#ifndef _NPU_TOOLKIT_PRINT_CONDITION_WARNING
#define _NPU_TOOLKIT_PRINT_CONDITION_WARNING(cond) NPU_TOOLKIT_INFO("Condition failed %s: %s:%d", #cond, __FILE__, __LINE__);
#endif


 /**
  * Check condition and print warning if check fails
  */
#define NPU_TOOLKIT_CHECK(condition) (condition) ? (void)0 : _NPU_TOOLKIT_PRINT_CONDITION_WARNING(condition)
 /**
  * Check that x == y, print warning if not
  */
#define NPU_TOOLKIT_CHECK_EQ(x, y) NPU_TOOLKIT_CHECK((x) == (y))
 /**
  * Check that x != y, print warning if not
  */
#define NPU_TOOLKIT_CHECK_NE(x, y) NPU_TOOLKIT_CHECK((x) != (y))
 /**
  * Check that x >= y, print warning if not
  */
#define NPU_TOOLKIT_CHECK_GE(x, y) NPU_TOOLKIT_CHECK((x) >= (y))
 /**
  * Check that x > y, print warning if not
  */
#define NPU_TOOLKIT_CHECK_GT(x, y) NPU_TOOLKIT_CHECK((x) > (y))
 /**
  * Check that x <= y, print warning if not
  */
#define NPU_TOOLKIT_CHECK_LE(x, y) NPU_TOOLKIT_CHECK((x) <= (y))
 /**
  * Check that x < y, print warning if not
  */
#define NPU_TOOLKIT_CHECK_LT(x, y) NPU_TOOLKIT_CHECK((x) < (y))




#ifndef NPU_TOOLKIT_LAYER_WARNING
  /**
   * Issue a warning for the active layer of the model.
   * A warning means that the model can fallback to a non-accelerated implementation, but there may be a performance impact.
   */
  #define NPU_TOOLKIT_LAYER_WARNING(fmt, ...) NPU_TOOLKIT_WARN("%s: " fmt, npu_toolkit::TfliteMicroModelHelper::current_layer_name(), ## __VA_ARGS__)
#endif

#ifndef NPU_TOOLKIT_LAYER_ERROR
  /**
   * Issue an error for the active layer of the model.
   * An error means that the model cannot be executed, and that execution should be aborted.
   */
  #define NPU_TOOLKIT_LAYER_ERROR(fmt, ...) NPU_TOOLKIT_ERROR("%s: " fmt, npu_toolkit::TfliteMicroModelHelper::current_layer_name(), ## __VA_ARGS__)
#endif


#ifndef NPU_TOOLKIT_ENSURE_STATUS
#define NPU_TOOLKIT_ENSURE_STATUS(x) \
  do {                           \
    const TfLiteStatus s = (x);  \
    if (s != kTfLiteOk) {        \
      return s;                  \
    }                            \
  } while (0)
#endif


#define NPU_TOOLKIT_ENSURE(context, x) \
if(!(x)){ \
  NPU_TOOLKIT_LAYER_WARNING("%s:%d %s was not true.", __FILE__, __LINE__, # x); \
  return kTfLiteError; \
}

#define NPU_TOOLKIT_ENSURE_EQ(context, a, b) \
if((a) != (b)) { \
  NPU_TOOLKIT_LAYER_WARNING("%s:%d %s != %s (%d != %d)", __FILE__, __LINE__, #a, #b, (a), (b)); \
  return kTfLiteError; \
}

#define NPU_TOOLKIT_ENSURE_NE(context, a, b) \
if((a) == (b)) { \
  NPU_TOOLKIT_LAYER_WARNING("%s:%d %s == %s (%d == %d)", __FILE__, __LINE__, #a, #b, (a), (b)); \
  return kTfLiteError; \
}

#define NPU_TOOLKIT_ENSURE_LE(context, a, b) \
if((a) > (b)) { \
  NPU_TOOLKIT_LAYER_WARNING("%s:%d %s > %s (%d > %d)", __FILE__, __LINE__, #a, #b, (a), (b)); \
  return kTfLiteError; \
}

#define NPU_TOOLKIT_ENSURE_GE(context, a, b) \
if((a) < (b)) { \
  NPU_TOOLKIT_LAYER_WARNING("%s:%d %s < %s (%d < %d)", __FILE__, __LINE__, #a, #b, (a), (b)); \
  return kTfLiteError; \
}

/**
 * Zero-point range in -128 to 127:
 * https://ai.google.dev/edge/litert/conversion/tensorflow/quantization/quantization_spec
 */
#define NPU_TOOLKIT_ENSURE_ZEROPOINT(context, zp) \
  NPU_TOOLKIT_ENSURE_LE(context, zp, 127); \
  NPU_TOOLKIT_ENSURE_GE(context, zp, -128)




// Define dummy macros if the given log level is greater
namespace {
  static inline void _UnusedLoggingFunction_(const char* format, ...){
    (void)format;
  }
}


#ifndef NPU_TOOLKIT_DEBUG
#define NPU_TOOLKIT_DEBUG _UnusedLoggingFunction_
#endif
#ifndef NPU_TOOLKIT_INFO
#define NPU_TOOLKIT_INFO _UnusedLoggingFunction_
#endif
#ifndef NPU_TOOLKIT_WARN
#define NPU_TOOLKIT_WARN _UnusedLoggingFunction_
#endif
#ifndef NPU_TOOLKIT_ERROR
#define NPU_TOOLKIT_ERROR _UnusedLoggingFunction_
#endif