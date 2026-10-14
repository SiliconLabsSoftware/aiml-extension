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

#include <cstdint>
#include "tflite_micro_logger.hpp"


namespace npu_toolkit
{

static inline uint16_t bytes_to_uint16(const uint8_t* bytes)
{
  return (((uint16_t)bytes[0]) << 0) | (((uint16_t)bytes[1]) << 8);
}

static inline uint32_t bytes_to_uint24(const uint8_t* bytes)
{
  return (((uint16_t)bytes[0]) << 0) | (((uint16_t)bytes[1]) << 8) | (((uint16_t)bytes[2]) << 16);
}


static inline uint32_t bytes_to_uint32(const uint8_t* bytes)
{
  return (((uint32_t)bytes[0]) << 0)  | (((uint32_t)bytes[1]) << 8) |
         (((uint32_t)bytes[2]) << 16) | (((uint32_t)bytes[3]) << 24);
}

template<typename T>
T align_up_uint32(T v)
{
  return ((v + 3) >> 2) << 2;
}

template<typename T>
T align_up_uint16(T v)
{
  return ((v + 1) >> 1) << 1;
}

template<typename T>
T align_down_n(T x, uint32_t n)
{
    return x & ~(n-1);
}

template<typename T>
T align_up_n(T x, uint32_t n)
{
    return (x + (n-1)) & ~(n-1);
}


#ifdef COMPILED_MODEL_PAGING_DEBUG_ENABLED
  #define DEBUG_PRINTF(fmt, ...) NPU_TOOLKIT_INFO("[CM] " fmt, ## __VA_ARGS__)
#else
  #define DEBUG_PRINTF(...)
#endif

#if defined(__clang__) && defined(__has_attribute)
  #if __has_attribute(optimize)
    #define COMPILED_MODEL_OPTIMIZE_ATTR __attribute__((optimize("O3")))
  #endif
#elif defined(__GNUC__)
#define COMPILED_MODEL_OPTIMIZE_ATTR __attribute__((optimize("O3")))
#endif

#ifndef COMPILED_MODEL_OPTIMIZE_ATTR
  #define COMPILED_MODEL_OPTIMIZE_ATTR
#endif


} // namespace npu_toolkit