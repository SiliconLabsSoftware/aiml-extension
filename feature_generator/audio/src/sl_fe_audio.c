/***************************************************************************//**
 * @file sl_fe_audio.c
 * @brief Silicon Labs audio feature generator (EFR32)
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
#include <stdlib.h>
#include <string.h>

#include "sl_status.h"
#include "sl_mic.h"
#include "microfrontend/lib/frontend.h"
#include "microfrontend/lib/frontend_util.h"
#include "sl_fe_audio.h"
#include "sl_fe_audio_common.h"
#include "sl_common.h"

/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/

/** Ping-pong mic DMA buffer (2 * chunk frames); sized in @ref sl_ml_audio_feature_generation_init. */
static int16_t *mic_sample_buffer;
static size_t mic_dma_chunk_size;

static size_t audio_buffer_cap;
static size_t feature_buffer_cap;

static int16_t *audio_buffer;
static uint16_t *feature_buffer;

static volatile size_t audio_buffer_read_index;
static volatile size_t audio_buffer_write_index;
static volatile size_t feature_buffer_start;

static size_t num_unfetched_slices = 0;

static int audio_volume_scaler;
static int quantize_dynamic_scale_range;

static sl_fe_audio_config_t sli_fe_runtime_cfg;
static bool sli_fe_runtime_cfg_valid;

/** True once @c frontend_init succeeds until @ref sl_ml_audio_feature_generation_deinit completes. */
static bool sli_ml_fe_pipeline_active = false;

struct FrontendState frontend_state;

/*******************************************************************************
 **************************   LOCAL FUNCTIONS   ********************************
 ******************************************************************************/

static sl_status_t process_audio_buffer_chunk(const int16_t *audio_data, size_t *slices_updated);
static void sli_ml_audio_feature_generation_release(void);
static void sli_ml_fe_mic_buffer_release(void);
static void sli_ml_fe_mic_teardown(void);

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

sl_status_t sl_ml_audio_feature_generation_frontend_init(const sl_fe_audio_config_t *config)
{
  if (config == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (config->sample_rate_hz <= 0 || config->frontend_config.window.step_size_ms == 0U
      || config->sample_length_ms <= config->frontend_config.window.size_ms
      || config->audio_buffer_chunk_size == 0U) {
    return SL_STATUS_FAIL;
  }

  sli_ml_audio_feature_generation_release();

  {
    const sl_status_t status = sli_fe_audio_compute_feature_buffer_element_count(config,
                                                                                 &feature_buffer_cap);
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  /* Ring buffer capacity must be a multiple of audio_buffer_chunk_size: each mic DMA
   * callback and each frontend step uses one chunk of contiguous samples from
   * audio_buffer[index]. Round up so chunk-aligned indices wrap within the buffer. */
  {
    const size_t raw_cap = config->audio_buffer_size;
    const size_t chunk = (size_t)config->audio_buffer_chunk_size;
    if (raw_cap == 0U) {
      return SL_STATUS_FAIL;
    }
    audio_buffer_cap = ((raw_cap + chunk - 1U) / chunk) * chunk;
  }

  audio_buffer = (int16_t *)calloc(audio_buffer_cap, sizeof(int16_t));
  feature_buffer = (uint16_t *)calloc(feature_buffer_cap, sizeof(uint16_t));
  if ((audio_buffer == NULL) || (feature_buffer == NULL)) {
    sli_ml_audio_feature_generation_release();
    return SL_STATUS_ALLOCATION_FAILED;
  }

  sli_fe_runtime_cfg = *config;
  sli_fe_runtime_cfg_valid = true;

  audio_volume_scaler = sli_fe_runtime_cfg.audio_gain;

  quantize_dynamic_scale_range = sli_fe_audio_compute_dynamic_scale_range(
      sli_fe_runtime_cfg.quantize_dynamic_scale_enable,
      sli_fe_runtime_cfg.quantize_dynamic_scale_range_db,
      (unsigned)sli_fe_runtime_cfg.frontend_config.log_scale.scale_shift);

  audio_buffer_read_index = 0;
  audio_buffer_write_index = 0;
  feature_buffer_start = 0;

  if (!FrontendPopulateState(&sli_fe_runtime_cfg.frontend_config,
                               &frontend_state,
                               sli_fe_runtime_cfg.sample_rate_hz)) {
    sli_ml_audio_feature_generation_release();
    return SL_STATUS_FAIL;
  }

  return SL_STATUS_OK;
}

sl_status_t sli_ml_audio_feature_generation_audio_buffer_write_chunk(const int16_t *new_samples, size_t num_samples)
{
  if (audio_buffer == NULL || !sli_fe_runtime_cfg_valid) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  if (num_samples != sli_fe_runtime_cfg.audio_buffer_chunk_size) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  for (uint32_t i = 0; i < num_samples; i++) {
    audio_buffer[audio_buffer_write_index + i] = new_samples[i] * audio_volume_scaler;
  }

  audio_buffer_write_index = (audio_buffer_write_index + num_samples) % audio_buffer_cap;
  return SL_STATUS_OK;
}

sl_status_t sl_ml_audio_feature_generation_update_features()
{
  size_t num_slices_updated = 0;

  if (audio_buffer == NULL || !sli_fe_runtime_cfg_valid) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  while (audio_buffer_read_index != audio_buffer_write_index) {
    process_audio_buffer_chunk(&audio_buffer[audio_buffer_read_index], &num_slices_updated);
    audio_buffer_read_index =
        (audio_buffer_read_index + sli_fe_runtime_cfg.audio_buffer_chunk_size) % audio_buffer_cap;
  }

  if (num_slices_updated == 0) {
    return SL_STATUS_EMPTY;
  }

  num_unfetched_slices += num_slices_updated;
  return SL_STATUS_OK;
}

sl_status_t sl_ml_audio_feature_generation_get_features_raw(uint16_t *buffer, size_t num_elements)
{
  if (!sli_fe_runtime_cfg_valid) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  if (num_elements != feature_buffer_cap) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  sli_fe_audio_features_to_u16(feature_buffer, feature_buffer_cap, feature_buffer_start, buffer);

  num_unfetched_slices = 0;
  return SL_STATUS_OK;
}

sl_status_t sli_ml_audio_feature_generation_get_features_quantized(int8_t *buffer,
                                                                 size_t num_elements,
                                                                 uint16_t range_min,
                                                                 uint16_t range_max)
{
  if (!sli_fe_runtime_cfg_valid) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  if (num_elements != feature_buffer_cap) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (range_min == range_max) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  sli_fe_audio_quantize_i8(feature_buffer, feature_buffer_cap, feature_buffer_start,
                           range_min, range_max, buffer);

  num_unfetched_slices = 0;
  return SL_STATUS_OK;
}

sl_status_t sli_ml_audio_feature_generation_get_features_dynamically_quantized(int8_t *buffer,
                                                                               size_t num_elements,
                                                                               uint16_t dynamic_range)
{
  if (!sli_fe_runtime_cfg_valid) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  if (num_elements != feature_buffer_cap) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (dynamic_range == 0) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  sli_fe_audio_dynamic_quantize_i8(feature_buffer, feature_buffer_cap, feature_buffer_start,
                                   dynamic_range, buffer);

  num_unfetched_slices = 0;
  return SL_STATUS_OK;
}

sl_status_t sl_ml_audio_feature_generation_fill_tensor(TfLiteTensor *input_tensor)
{
  sl_status_t status = SL_STATUS_OK;
  if (!sli_fe_runtime_cfg_valid) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  if (input_tensor->type == kTfLiteInt8) {
    const size_t n_int8 = input_tensor->bytes / sizeof(int8_t);

    if (sli_fe_runtime_cfg.quantize_dynamic_scale_enable != 0) {
      status = sli_ml_audio_feature_generation_get_features_dynamically_quantized(input_tensor->data.int8,
                                                                                n_int8,
                                                                                (uint16_t)quantize_dynamic_scale_range);
    } else {
      status = sli_ml_audio_feature_generation_get_features_quantized(input_tensor->data.int8,
                                                                      n_int8,
                                                                      sli_fe_runtime_cfg.quantize_feature_range_min,
                                                                      sli_fe_runtime_cfg.quantize_feature_range_max);
    }

  } else {
    status = SL_STATUS_INVALID_PARAMETER;
  }

  return status;
}

int sl_ml_audio_feature_generation_get_new_feature_slice_count()
{
  return (int)num_unfetched_slices;
}

void sl_ml_audio_feature_generation_reset()
{
  if (feature_buffer != NULL && feature_buffer_cap > 0U) {
    memset(feature_buffer, 0, feature_buffer_cap * sizeof(uint16_t));
  }
  if (audio_buffer != NULL && audio_buffer_cap > 0U) {
    memset(audio_buffer, 0, audio_buffer_cap * sizeof(int16_t));
  }

  audio_buffer_read_index = 0;
  audio_buffer_write_index = 0;
  feature_buffer_start = 0;

  num_unfetched_slices = 0;

  if (sli_fe_runtime_cfg_valid) {
    FrontendReset(&frontend_state);
  }
}

int sl_ml_audio_feature_generation_get_feature_buffer_size()
{
  return (int)feature_buffer_cap;
}

static sl_status_t process_audio_buffer_chunk(const int16_t *audio_data, size_t *slices_updated)
{
  size_t num_samples = sli_fe_runtime_cfg.audio_buffer_chunk_size;

  while (num_samples > 0) {
    size_t num_samples_read;
    struct FrontendOutput output = FrontendProcessSamples(
      &frontend_state, audio_data, num_samples, &num_samples_read);

    audio_data += num_samples_read;
    num_samples -= num_samples_read;
    if (output.values != NULL) {
      for (size_t i = 0; i < output.size; i++) {
        feature_buffer[(feature_buffer_start + i) % feature_buffer_cap] = output.values[i];
      }

      feature_buffer_start = (feature_buffer_start + output.size) % feature_buffer_cap;
      *slices_updated += 1;
    }
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 *  Callback invoked when a chunk of mic audio samples is ready.
 ******************************************************************************/
static void mic_buffer_ready_callback(const int16_t *buffer, uint32_t n_frames)
{
  sli_ml_audio_feature_generation_audio_buffer_write_chunk(buffer, n_frames);
}

/***************************************************************************//**
 *  Set up the microphone and initialize the audio feature generation frontend.
 ******************************************************************************/
sl_status_t sl_ml_audio_feature_generation_deinit(void)
{
  if (!sli_ml_fe_pipeline_active) {
    return SL_STATUS_OK;
  }

  const sl_status_t mic_st = sl_mic_deinit();
  sli_ml_audio_feature_generation_release();
  sli_ml_fe_mic_buffer_release();
  sli_ml_fe_pipeline_active = false;
  return mic_st;
}

static void sli_ml_audio_feature_generation_release(void)
{
  if (sli_fe_runtime_cfg_valid) {
    FrontendReset(&frontend_state);
  }

  free(audio_buffer);
  free(feature_buffer);
  audio_buffer = NULL;
  feature_buffer = NULL;
  audio_buffer_cap = 0U;
  feature_buffer_cap = 0U;
  audio_buffer_read_index = 0;
  audio_buffer_write_index = 0;
  feature_buffer_start = 0;
  num_unfetched_slices = 0;
  sli_fe_runtime_cfg_valid = false;
  quantize_dynamic_scale_range = 0;
}

static void sli_ml_fe_mic_buffer_release(void)
{
  free(mic_sample_buffer);
  mic_sample_buffer = NULL;
  mic_dma_chunk_size = 0U;
}

static void sli_ml_fe_mic_teardown(void)
{
  (void)sl_mic_deinit();
  sli_ml_fe_mic_buffer_release();
}

sl_status_t sl_ml_audio_feature_generation_init(const sl_fe_audio_config_t *config)
{
  sl_status_t mic_status;

  if (sli_ml_fe_pipeline_active) {
    const sl_status_t td = sl_ml_audio_feature_generation_deinit();
    if (td != SL_STATUS_OK) {
      return td;
    }
  }

  mic_status = sl_ml_audio_feature_generation_frontend_init(config);
  if (mic_status != SL_STATUS_OK) {
    return mic_status;
  }

  const size_t chunk = (size_t)config->audio_buffer_chunk_size;

  if (mic_dma_chunk_size != chunk) {
    sli_ml_fe_mic_buffer_release();
  }
  if (mic_sample_buffer == NULL) {
    mic_sample_buffer = (int16_t *)malloc(2U * chunk * sizeof(int16_t));
    if (mic_sample_buffer == NULL) {
      sli_ml_audio_feature_generation_release();
      return SL_STATUS_ALLOCATION_FAILED;
    }
    mic_dma_chunk_size = chunk;
  }

  mic_status = sl_mic_init(config->sample_rate_hz, 1);
  if (mic_status != SL_STATUS_OK) {
    sli_ml_audio_feature_generation_release();
    sli_ml_fe_mic_buffer_release();
    return mic_status;
  }

  mic_status = sl_mic_start_streaming(mic_sample_buffer,
                                      (uint32_t)chunk,
                                      (sl_mic_buffer_ready_callback_t)mic_buffer_ready_callback);

  if (mic_status != SL_STATUS_OK) {
    sli_ml_fe_mic_teardown();
    sli_ml_audio_feature_generation_release();
    return mic_status;
  }

  sli_ml_fe_pipeline_active = true;
  return SL_STATUS_OK;
}
