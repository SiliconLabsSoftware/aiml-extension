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

#include <algorithm>
#include <cassert>

#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/kernels/internal/types.h"
#include "tensorflow/lite/c/builtin_op_data.h"
#include "tflite_micro_logger.hpp"


namespace npu_toolkit
{

/**
 * Convert TfLitePadding -> tflite::PaddingType
 */
static inline tflite::PaddingType runtime_padding_type(TfLitePadding padding)
{
    switch (padding)
    {
    case TfLitePadding::kTfLitePaddingSame:
        return tflite::PaddingType::kSame;
    case TfLitePadding::kTfLitePaddingValid:
        return tflite::PaddingType::kValid;
    case TfLitePadding::kTfLitePaddingUnknown:
    default:
        return tflite::PaddingType::kNone;
    }
}

/**
 * Convert tflite::PaddingType -> TfLitePadding
 */
static inline TfLitePadding runtime_padding_type(tflite::PaddingType padding)
{
  switch(padding)
  {
    case tflite::PaddingType::kSame:
      return TfLitePadding::kTfLitePaddingSame;
    case tflite::PaddingType::kValid:
      return TfLitePadding::kTfLitePaddingValid;
    case tflite::PaddingType::kNone:
    default:
      return TfLitePadding::kTfLitePaddingUnknown;
  }
}

/**
 * Integer divide and round up
 */
inline int div_round_up(const int numerator, const int denominator)
{
    return (numerator + denominator - 1) / denominator;
}

/**
 * Divide and round up with lower limit
 */
inline int div_ceil(const int numerator, const int denominator, int lower_limit = std::numeric_limits<int>::min())
{
    assert(denominator != 0 or !"denominator must not be zero");
    int ceil = 0;

    const std::div_t div = std::div(numerator, denominator);

    auto sign = [](int n){ return n > 0 ? 1 : n < 0 ? -1 : 0; };
    if (sign(numerator) != sign(denominator)) {
        ceil = div.quot;  // negative number already truncated towards 0
    } else { // exact 0 or positive number truncated toward 0
        ceil = div.quot + (div.rem ? 1 : 0);
    }

    return std::max(ceil, lower_limit);
}

/**
 * Integer divide and round down with a upper limit
 */
inline int div_floor(const int numerator, const int denominator, int upper_limit = std::numeric_limits<int>::max())
{
    assert(denominator != 0);
    const std::div_t div = std::div(numerator, denominator);

    auto sign = [](int n){ return n > 0 ? 1 : n < 0 ? -1 : 0; };
    int floor;
    if (sign(numerator) == sign(denominator)) {
        floor = div.quot;  // positive number already truncated towards 0
    } else { // exact 0 or negative number truncated toward 0
        floor = div.quot - (div.rem ? 1 : 0);
    }

    return std::min(floor, upper_limit);
}


} // namespace npu_toolkit