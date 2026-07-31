/***************************************************************************//**
 * @file sl_fe_audio_common.c
 * @brief Shared (toolchain-neutral) audio feature math for the fe_audio component
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include <math.h>
#include <stdint.h>

#include "sl_fe_audio_common.h"
#include "sl_status.h"

sl_status_t sli_fe_audio_validate_config(const sl_fe_audio_config_t *config)
{
  if (config == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (config->audio_buffer_size == 0U) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (config->sample_rate_hz <= 0 || config->frontend_config.window.step_size_ms == 0U
      || config->sample_length_ms <= config->frontend_config.window.size_ms) {
    return SL_STATUS_FAIL;
  }
  return SL_STATUS_OK;
}

sl_status_t sli_fe_audio_compute_feature_buffer_element_count(const sl_fe_audio_config_t *config,
                                                              size_t *element_count)
{
  if (element_count == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  const sl_status_t cfg_st = sli_fe_audio_validate_config(config);
  if (cfg_st != SL_STATUS_OK) {
    return cfg_st;
  }

  const unsigned wsize = (unsigned)config->frontend_config.window.size_ms;
  const unsigned wstep = (unsigned)config->frontend_config.window.step_size_ms;
  const unsigned nch = (unsigned)config->frontend_config.filterbank.num_channels;
  if (nch == 0U) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  const unsigned slice_count = 1U + ((config->sample_length_ms - wsize) / wstep);
  if (slice_count > (SIZE_MAX / (size_t)nch)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  *element_count = (size_t)slice_count * (size_t)nch;
  return SL_STATUS_OK;
}

/* Explicit clamp helpers keep the quantizer output bit-identical regardless of which
 * platform macro (`SL_MAX`/`SL_MIN`) or C++ helper (`std::max`/`std::min`) the callers
 * previously used; this single core must match both. */
static int32_t sli_fe_audio_max_i32(int32_t a, int32_t b)
{
  return (a > b) ? a : b;
}

static int32_t sli_fe_audio_min_i32(int32_t a, int32_t b)
{
  return (a < b) ? a : b;
}

int sli_fe_audio_compute_dynamic_scale_range(int dynamic_scale_enable,
                                             float dynamic_scale_range_db,
                                             unsigned log_scale_shift)
{
  if (dynamic_scale_enable == 0) {
    return 0;
  }

#define LN10_DIV_20 0.11512925465f
  return (int)(dynamic_scale_range_db
               * (float)(1U << log_scale_shift)
               * LN10_DIV_20);
#undef LN10_DIV_20
}

void sli_fe_audio_features_to_u16(const uint16_t *feature_buffer,
                                  size_t count,
                                  size_t start,
                                  uint16_t *out)
{
  for (size_t i = 0; i < count; i++) {
    const size_t capture_index = (start + i) % count;
    out[i] = feature_buffer[capture_index];
  }
}

void sli_fe_audio_features_to_f32(const uint16_t *feature_buffer,
                                  size_t count,
                                  size_t start,
                                  float *out)
{
  for (size_t i = 0; i < count; i++) {
    const size_t capture_index = (start + i) % count;
    out[i] = (float)feature_buffer[capture_index];
  }
}

void sli_fe_audio_quantize_i8(const uint16_t *feature_buffer,
                              size_t count,
                              size_t start,
                              uint16_t range_min,
                              uint16_t range_max,
                              int8_t *out)
{
  const int32_t value_scale = 256;
  const int32_t value_div = (int32_t)range_max - (int32_t)range_min;

  for (size_t i = 0; i < count; i++) {
    const size_t capture_index = (start + i) % count;
    int32_t value = (((int32_t)feature_buffer[capture_index] - (int32_t)range_min) * value_scale
                     + (value_div / 2))
                    / value_div;
    value -= 128;
    if (value < -128) {
      value = -128;
    }
    if (value > 127) {
      value = 127;
    }
    out[i] = (int8_t)value;
  }
}

void sli_fe_audio_dynamic_quantize_i8(const uint16_t *feature_buffer,
                                      size_t count,
                                      size_t start,
                                      uint16_t dynamic_range,
                                      int8_t *out)
{
  int32_t maxval = 0;
  for (size_t i = 0; i < count; i++) {
    const size_t capture_index = (start + i) % count;
    const int32_t value = (int32_t)feature_buffer[capture_index];
    maxval = sli_fe_audio_max_i32(value, maxval);
  }

  const int32_t minval = sli_fe_audio_max_i32(maxval - (int32_t)dynamic_range, 0);
  const int32_t val_range = sli_fe_audio_max_i32(maxval - minval, 1);

  for (size_t i = 0; i < count; i++) {
    const size_t capture_index = (start + i) % count;
    int32_t value = (int32_t)feature_buffer[capture_index];
    value -= minval;
    value *= 255;
    value /= val_range;
    value -= 128;
    value = sli_fe_audio_min_i32(sli_fe_audio_max_i32(value, -128), 127);
    out[i] = (int8_t)value;
  }
}

void sli_fe_audio_scale_f32(const uint16_t *feature_buffer,
                            size_t count,
                            size_t start,
                            float scaler,
                            float *out)
{
  for (size_t i = 0; i < count; i++) {
    const size_t capture_index = (start + i) % count;
    const float value = (float)feature_buffer[capture_index];
    out[i] = value * scaler;
  }
}

void sli_fe_audio_mean_std_normalize_f32(const uint16_t *feature_buffer,
                                         size_t count,
                                         size_t start,
                                         float *out)
{
  float mean = 0.0f;
  float sample_count = 0.0f;
  float m2 = 0.0f;

  /* Welford's online mean/variance over the buffer in logical order. */
  for (size_t i = 0; i < count; i++) {
    const size_t capture_index = (start + i) % count;
    const float value = (float)feature_buffer[capture_index];

    sample_count += 1.0f;

    const float delta = value - mean;
    mean += delta / sample_count;
    const float delta2 = value - mean;
    m2 += delta * delta2;
  }

  const float variance = m2 / sample_count;
  const float std = sqrtf(variance);
  /* Constant (or near-constant) spectrogram: population std is zero; avoid 1/std. */
  static const float k_std_epsilon = 1e-6f;

  if (std < k_std_epsilon) {
    for (size_t i = 0; i < count; i++) {
      out[i] = 0.0f;
    }
  } else {
    const float std_recip = 1.0f / std;
    for (size_t i = 0; i < count; i++) {
      const size_t capture_index = (start + i) % count;
      const float value = (float)feature_buffer[capture_index];
      const float x = value - mean;
      out[i] = x * std_recip;
    }
  }
}
