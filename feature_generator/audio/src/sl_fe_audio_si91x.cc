/***************************************************************************//**
 * @file sl_fe_audio_si91x.cc
 * @brief Silicon Labs audio feature generator (Si91x)
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
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <algorithm>
#include "sl_fe_audio_si91x.h"
#include "sl_fe_audio_common.h"
#include "sl_status.h"
#include "microfrontend/lib/frontend.h"
#include "microfrontend/lib/frontend_util.h"
#include "ml/third_party/tflm/common.h"
#include "sl_ml_mic_i2s_si91x.h"

/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/

// Audio buffer to store long term audio data, needs to be big enough to store 100ms of data
int16_t* sl_ml_audio_feature_generation_audio_buffer;
static int16_t* audio_buffer;

// Ring buffer to store feature
static uint16_t* feature_buffer;

/* Ring indices may be updated from the mic/I2S path vs the main thread; volatile limits
 * reordering/visibility assumptions until a single documented memory model is used. */
static volatile size_t audio_buffer_read_index;
static volatile size_t audio_buffer_write_index;
static volatile size_t feature_buffer_read_index;
static volatile size_t feature_buffer_write_index;

// Counter to maintain number of new and available slices
static size_t num_unfetched_slices = 0;

// Optional audio volume scaling factor
static int audio_volume_scaler;

static int quantize_dynamic_scale_range;

static size_t audio_buffer_size;
static size_t feature_buffer_size;

static sl_fe_audio_config_t sli_fe_runtime_cfg;
static bool sli_fe_runtime_cfg_valid;

/** True once @c frontend_init succeeds until @ref sl_ml_audio_feature_generation_deinit completes. */
static bool sli_ml_fe_si91x_pipeline_active = false;
/** Non-NULL only after @ref sl_ml_audio_feature_generation_init (heap audio buffer). */
static int16_t *sli_ml_fe_si91x_owned_heap_buffer = NULL;

static sl_ml_audio_feature_generation_mic_callback_t mic_callback = NULL;
static void *mic_callback_arg = NULL;

// Frontend State
struct FrontendState frontend_state;

extern "C" void sli_ml_audio_feature_generation_si91x_release(void);

/*******************************************************************************
 **************************   LOCAL FUNCTIONS   ********************************
 ******************************************************************************/

static sl_status_t process_audio_buffer_chunk(const int16_t *audio_data, size_t num_samples, size_t *slices_updated);

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

sl_status_t sl_ml_audio_feature_generation_frontend_init(const sl_fe_audio_config_t *config)
{
  size_t feature_buffer_element_count = 0U;
  const sl_status_t status = sli_fe_audio_compute_feature_buffer_element_count(
      config, &feature_buffer_element_count);
  if (status != SL_STATUS_OK) {
    return status;
  }

  sli_ml_audio_feature_generation_si91x_release();

  audio_buffer = sl_ml_audio_feature_generation_audio_buffer;
  if (audio_buffer == NULL) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  uint16_t *new_feat = (uint16_t *)calloc(feature_buffer_element_count, sizeof(uint16_t));
  if (new_feat == NULL) {
    return SL_STATUS_ALLOCATION_FAILED;
  }

  feature_buffer = new_feat;
  feature_buffer_size = feature_buffer_element_count;
  audio_buffer_size = config->audio_buffer_size;

  sli_fe_runtime_cfg = *config;
  sli_fe_runtime_cfg_valid = true;

  audio_volume_scaler = sli_fe_runtime_cfg.audio_gain;

  quantize_dynamic_scale_range = sli_fe_audio_compute_dynamic_scale_range(
      sli_fe_runtime_cfg.quantize_dynamic_scale_enable,
      sli_fe_runtime_cfg.quantize_dynamic_scale_range_db,
      (unsigned)sli_fe_runtime_cfg.frontend_config.log_scale.scale_shift);

  audio_buffer_read_index = 0;
  audio_buffer_write_index = 0;
  feature_buffer_read_index = 0;
  feature_buffer_write_index = 0;

  if (!FrontendPopulateState(&sli_fe_runtime_cfg.frontend_config,
                               &frontend_state,
                               sli_fe_runtime_cfg.sample_rate_hz)) {
    sli_ml_audio_feature_generation_si91x_release();
    return SL_STATUS_FAIL;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 *  Mic chunk notification: samples already live in @c sl_ml_audio_feature_generation_audio_buffer
 *  (I2S/DMA ping-pong). Advances the ring write index and optionally applies @c audio_gain
 *  in place when gain is greater than 1. @p new_samples points at the filled region (same
 *  backing store); it is not memcpy'd into the ring by this function.
 *
 *  @param[in] new_samples
 *    Pointer to the chunk that was just written into the audio ring (same buffer base).
 *
 *  @param[in] num_samples
 *    Number of samples in this chunk; must stay consistent across callbacks for a given init.
 *
 *  @return
 *    SL_STATUS_OK on success, or an error status if not initialized.
 *
 ******************************************************************************/
sl_status_t sli_ml_audio_feature_generation_audio_buffer_write_chunk(const int16_t *new_samples, size_t num_samples)
{
  if (audio_buffer == NULL || audio_buffer_size == 0U) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  if (audio_volume_scaler > 1) {
    size_t index = audio_buffer_write_index;
    for (uint32_t i = 0; i < num_samples; i++) {
      audio_buffer[index] = (int16_t)((int32_t)audio_buffer[index] * audio_volume_scaler);
      index = (index + 1U) % audio_buffer_size;
    }
  }

  audio_buffer_write_index = (audio_buffer_write_index + num_samples) % audio_buffer_size;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 *  Updates the feature buffer with as many feature slices as can be calculated
 *  with new audio data.
 ******************************************************************************/
sl_status_t sl_ml_audio_feature_generation_update_features()
{
  sl_ml_mic_process_action();

  if (audio_buffer == NULL || audio_buffer_size == 0U || feature_buffer == NULL
      || !sli_fe_runtime_cfg_valid) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  size_t num_slices_updated = 0;

  // Ensure audio has been written before we start processing
  if(audio_buffer_read_index == audio_buffer_write_index)
  {
    return SL_STATUS_EMPTY;
  }

  int new_data_length = (audio_buffer_read_index < audio_buffer_write_index)
                            ? (int)(audio_buffer_write_index - audio_buffer_read_index)
                            : (int)((audio_buffer_size - audio_buffer_read_index) + audio_buffer_write_index);

  if (new_data_length > 0) {
    const int length_to_end = (int)(audio_buffer_size - audio_buffer_read_index);
    const int chunk_length = (new_data_length < length_to_end) ? new_data_length : length_to_end;

    process_audio_buffer_chunk(&audio_buffer[audio_buffer_read_index], (size_t)chunk_length, &num_slices_updated);

    // Update read index
    audio_buffer_read_index = (audio_buffer_read_index + (size_t)chunk_length) % audio_buffer_size;
    new_data_length -= chunk_length;

    // If the buffer wrapped and there's still more data to be read
    if (new_data_length > 0) {
      process_audio_buffer_chunk(&audio_buffer[0], (size_t)new_data_length, &num_slices_updated);

      audio_buffer_read_index = (size_t)new_data_length;
    }
  }

  if (num_slices_updated == 0) {
    // No new slices could be calculated
    return SL_STATUS_EMPTY;
  }

  num_unfetched_slices += num_slices_updated;


  // Could return SL_STATUS_NOT_READY if not full
  return SL_STATUS_OK;
}

/***************************************************************************//**
 *  Retrieves the features as type uint16 and copies them to the provided buffer.
 ******************************************************************************/
sl_status_t sl_ml_audio_feature_generation_get_features_raw(uint16_t *buffer, size_t num_elements)
{
  if (feature_buffer == NULL) {
    return SL_STATUS_NOT_INITIALIZED;
  }
  if (num_elements != feature_buffer_size) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  sli_fe_audio_features_to_u16(feature_buffer, feature_buffer_size, feature_buffer_read_index, buffer);

  num_unfetched_slices = 0;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 *  Retrieves the features as type float and copies them to the provided buffer.
 ******************************************************************************/
sl_status_t sl_ml_audio_feature_generation_get_features_raw_float32(float *buffer, size_t num_elements)
{
  if (feature_buffer == NULL) {
    return SL_STATUS_NOT_INITIALIZED;
  }
  if (num_elements != feature_buffer_size) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  sli_fe_audio_features_to_f32(feature_buffer, feature_buffer_size, feature_buffer_read_index, buffer);

  num_unfetched_slices = 0;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * @brief
 *    Retrieves the features quantized to signed integer numbers in the range
 *    -128 to 127 (int8) and copies them to the provided buffer.
 *
 *    @ref range_min and @ref range_max set the valid range of which values are
 *    quantized from.
 ******************************************************************************/
sl_status_t sli_ml_audio_feature_generation_get_features_quantized(int8_t *buffer,
                                                                   size_t num_elements,
                                                                   uint16_t range_min,
                                                                   uint16_t range_max)
{
  if (feature_buffer == NULL) {
    return SL_STATUS_NOT_INITIALIZED;
  }
  if (num_elements != feature_buffer_size) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (range_min == range_max) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  sli_fe_audio_quantize_i8(feature_buffer, feature_buffer_size, feature_buffer_read_index,
                           range_min, range_max, buffer);

  num_unfetched_slices = 0;

  return SL_STATUS_OK;
}


/***************************************************************************//**
 * @brief
 *     This converts the uint16 spectrograms to int8 using dynamic scaling
 *
 *    @ref dynamic_range the dynamic range of uint16 spectrogram to be mapped to int8
 *    dynamic_range = DYNAMIC_RANGE_DB*(2^log_scale_shift)*ln(10)/20
 *    300 corresponds to a DYNAMIC_RANGE_DB of 40 dB
 ******************************************************************************/
sl_status_t sli_ml_audio_feature_generation_get_features_dynamically_quantized(int8_t *buffer,
                                                                               size_t num_elements,
                                                                               uint16_t dynamic_range)
{
  if (feature_buffer == NULL) {
    return SL_STATUS_NOT_INITIALIZED;
  }
  if (num_elements != feature_buffer_size) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (dynamic_range == 0) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  sli_fe_audio_dynamic_quantize_i8(feature_buffer, feature_buffer_size, feature_buffer_read_index,
                                   dynamic_range, buffer);

  num_unfetched_slices = 0;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * buffer = (float)uint16_features_data / scaler
 ******************************************************************************/
sl_status_t sl_ml_audio_feature_generation_get_features_scaled(float *buffer, size_t num_elements, float scaler)
{
  if (feature_buffer == NULL) {
    return SL_STATUS_NOT_INITIALIZED;
  }
  if (num_elements != feature_buffer_size) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (scaler == 0.0f) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  sli_fe_audio_scale_f32(feature_buffer, feature_buffer_size, feature_buffer_read_index, scaler, buffer);

  num_unfetched_slices = 0;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 *  buffer = ((float)uint16_features_data - mean(uint16_features_data)) / std(uint16_features_data)
 ******************************************************************************/
sl_status_t sl_ml_audio_feature_generation_get_features_mean_std_normalized(float *buffer, size_t num_elements)
{
  if (feature_buffer == NULL) {
    return SL_STATUS_NOT_INITIALIZED;
  }
  if (num_elements != feature_buffer_size) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  sli_fe_audio_mean_std_normalize_f32(feature_buffer, feature_buffer_size, feature_buffer_read_index, buffer);

  num_unfetched_slices = 0;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 *  Fills a TensorFlow tensor with feature data
 ******************************************************************************/
sl_status_t sl_ml_audio_feature_generation_fill_tensor(struct TfLiteTensor *input_tensor)
{
  sl_status_t status = SL_STATUS_OK;
  if (input_tensor->type == kTfLiteInt8) {
    const size_t n_int8 = input_tensor->bytes / sizeof(int8_t);
    if (sli_fe_runtime_cfg.quantize_dynamic_scale_enable != 0) {
      status = sli_ml_audio_feature_generation_get_features_dynamically_quantized(
          input_tensor->data.int8,
          n_int8,
          (uint16_t)quantize_dynamic_scale_range);
    } else {
      status = sli_ml_audio_feature_generation_get_features_quantized(input_tensor->data.int8,
                                                                      n_int8,
                                                                      sli_fe_runtime_cfg.quantize_feature_range_min,
                                                                      sli_fe_runtime_cfg.quantize_feature_range_max);
    }

  } else if (input_tensor->type == kTfLiteUInt16) {
    const int n_elements = (int)(input_tensor->bytes / sizeof(uint16_t));
    status = sl_ml_audio_feature_generation_get_features_raw(input_tensor->data.ui16, (size_t)n_elements);

  } else if (input_tensor->type == kTfLiteFloat32) {
    const int n_elements = (int)(input_tensor->bytes / sizeof(float));

    if (sli_fe_runtime_cfg.samplewise_norm_rescale != 0.0f) {
      status = sl_ml_audio_feature_generation_get_features_scaled(
          input_tensor->data.f,
          (size_t)n_elements,
          sli_fe_runtime_cfg.samplewise_norm_rescale);
    } else if (sli_fe_runtime_cfg.samplewise_norm_mean_and_std != 0) {
      status = sl_ml_audio_feature_generation_get_features_mean_std_normalized(input_tensor->data.f, (size_t)n_elements);
    } else {
      status = sl_ml_audio_feature_generation_get_features_raw_float32(input_tensor->data.f, (size_t)n_elements);
    }

  } else {
    status = SL_STATUS_INVALID_PARAMETER;
  }

  return status;
}

/***************************************************************************//**
 *    Returns how many new or unfetched feature slices that have been updated
 *    since last call to sl_ml_audio_feature_generation_get_features_raw or
 *    sl_ml_audio_feature_generation_fill_tensor;
 ******************************************************************************/
int sl_ml_audio_feature_generation_get_new_feature_slice_count()
{
  return (int)num_unfetched_slices;
}

void sl_ml_audio_feature_generation_reset(void)
{
  if (feature_buffer != NULL && feature_buffer_size > 0U) {
    memset(feature_buffer, 0, feature_buffer_size * sizeof(uint16_t));
  }
  if (audio_buffer != NULL && audio_buffer_size > 0U) {
    memset(audio_buffer, 0, audio_buffer_size * sizeof(int16_t));
  }

  // Reset ring-buffer indices
  audio_buffer_read_index = 0;
  audio_buffer_write_index = 0;
  feature_buffer_read_index = 0;
  feature_buffer_write_index = 0;

  // Reset slice counter
  num_unfetched_slices = 0;

  if (sli_fe_runtime_cfg_valid) {
    FrontendReset(&frontend_state);
  }
}

/***************************************************************************//**
 *    Returns the size of the feature buffer
 ******************************************************************************/
int sl_ml_audio_feature_generation_get_feature_buffer_size()
{
  return (feature_buffer_size > (size_t)INT_MAX) ? INT_MAX : (int)feature_buffer_size;
}

/***************************************************************************//**
 *   Return if the activity detection block detected activity in the audio stream.
 ******************************************************************************/
sl_status_t sl_ml_audio_feature_generation_activity_detected()
{
  if (!sli_fe_runtime_cfg_valid
      || !sli_fe_runtime_cfg.frontend_config.activity_detection.enable_activation_detection) {
    return SL_STATUS_NOT_AVAILABLE;
  }

  return ActivityDetectionTripped(&frontend_state.activity_detection) ? SL_STATUS_OK : SL_STATUS_IN_PROGRESS;
}

bool sl_ml_audio_feature_generation_is_activity_detection_enabled(void)
{
  return sli_fe_runtime_cfg_valid && sli_fe_runtime_cfg.frontend_config.activity_detection.enable_activation_detection;
}

/*******************************************************************************
 **************************   LOCAL FUNCTIONS   ********************************
 ******************************************************************************/

/***************************************************************************//**
 *  Calculates features from a single chunk from the audio buffer
 *
 *  This function will consume all samples from the audio buffer chunk
 *  and generate as many feature slices as possible from this data. Any leftover
 *  audio samples will be consumed by Microfrontend and kept until the next call
 *  to generate features from then.
 *
 *  @param[in] audio_data
 *    Next chunk from the audio_data buffer to process
 *
 * @param[in] num_samples
 *    Number of samples to process in the audio_data buffer
 *
 *  @param[out] slices_updated
 *    Adds how many slices were updated to this parameter
 ******************************************************************************/
static sl_status_t process_audio_buffer_chunk(const int16_t *audio_data, size_t num_samples, size_t *slices_updated)
{
  if (audio_data == NULL || feature_buffer == NULL || feature_buffer_size == 0U || !sli_fe_runtime_cfg_valid) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  int write_index = (int)feature_buffer_write_index;

  while (num_samples > 0) {
    size_t num_samples_read;
    struct FrontendOutput output = FrontendProcessSamples(
        &frontend_state, audio_data, num_samples, &num_samples_read);

    audio_data += num_samples_read;
    num_samples -= num_samples_read;
    if (output.values != NULL) {
      for (size_t i = 0; i < output.size; i++) {
        feature_buffer[write_index + (int)i] = output.values[i];
      }

      write_index = (int)(((size_t)write_index + output.size) % feature_buffer_size);
      *slices_updated += 1;
    } else {
      // No feature slice was generated, but data is stored internally in the frontend for
      // next iteration.
    }
  }

  feature_buffer_write_index = (size_t)write_index;

  // Currently, we assume that the entire feature buffer is always processed
  // each time a sl_ml_audio_feature_generation_get_features_xxx API is called.
  // Thus, we set the read index to point the the next write index
  // (which is the oldest element in the circular buffer).
  // In this way, the sl_ml_audio_feature_generation_get_features_xxx APIs
  // will always return the latest features.
  // This helps to work-around dropped data.
  feature_buffer_read_index = feature_buffer_write_index;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 *  Callback invoked when a chunk of mic audio samples is ready.
 ******************************************************************************/
static void mic_buffer_ready_callback(const int16_t *buffer, uint32_t n_frames)
{
  if (mic_callback != NULL) {
    mic_callback(mic_callback_arg, buffer, n_frames);
  }
  (void)sli_ml_audio_feature_generation_audio_buffer_write_chunk(buffer, n_frames);
}

/***************************************************************************//**
 *  Initializes the audio feature generation pipeline with a provided buffer.
 ******************************************************************************/
sl_status_t sl_ml_audio_feature_generation_init_with_buffer(int16_t *buffer,
                                                            int n_frames,
                                                            const sl_fe_audio_config_t *config)
{
  sl_status_t mic_status;

  if (config == NULL || buffer == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (n_frames < 2 || (size_t)n_frames != config->audio_buffer_size) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (sli_ml_fe_si91x_pipeline_active) {
    const sl_status_t td = sl_ml_audio_feature_generation_deinit();
    if (td != SL_STATUS_OK) {
      return td;
    }
  }

  sl_ml_audio_feature_generation_audio_buffer = buffer;

  /* Validate @p config and allocate the feature ring before mic/DMA setup. */
  const sl_status_t fe_status = sl_ml_audio_feature_generation_frontend_init(config);
  if (fe_status != SL_STATUS_OK) {
    sl_ml_audio_feature_generation_audio_buffer = NULL;
    return fe_status;
  }

  mic_status = sl_ml_mic_init(config->sample_rate_hz, 1);
  if (mic_status != SL_STATUS_OK) {
    sli_ml_audio_feature_generation_si91x_release();
    sl_ml_audio_feature_generation_audio_buffer = NULL;
    return mic_status;
  }

  sli_ml_fe_si91x_pipeline_active = true;

  // The sl_mic library uses "ping-ponging" hence we divide by 2.
  // Start streaming but do not start the first transfer yet; let the state machine do it.
  mic_status = sl_ml_mic_start_streaming(sl_ml_audio_feature_generation_audio_buffer,
                                         n_frames / 2,
                                         (sl_mic_buffer_ready_callback_t)mic_buffer_ready_callback);

  if (mic_status != SL_STATUS_OK) {
    (void)sl_ml_audio_feature_generation_deinit();
    return mic_status;
  }

  mic_status = sl_ml_mic_process_action();
  if (mic_status != SL_STATUS_OK) {
    (void)sl_ml_audio_feature_generation_deinit();
    return mic_status;
  }

  return SL_STATUS_OK;
}

sl_status_t sl_ml_audio_feature_generation_deinit(void)
{
  if (!sli_ml_fe_si91x_pipeline_active) {
    return SL_STATUS_OK;
  }

  const sl_status_t mic_st = sl_ml_mic_deinit();
  mic_callback = NULL;
  mic_callback_arg = NULL;
  sli_ml_audio_feature_generation_si91x_release();
  sl_ml_audio_feature_generation_audio_buffer = NULL;
  if (sli_ml_fe_si91x_owned_heap_buffer != NULL) {
    free(sli_ml_fe_si91x_owned_heap_buffer);
    sli_ml_fe_si91x_owned_heap_buffer = NULL;
  }
  sli_ml_fe_si91x_pipeline_active = false;
  return mic_st;
}

sl_status_t sl_ml_audio_feature_generation_init(const sl_fe_audio_config_t *config)
{
  if (config == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (config->audio_buffer_size < 2U) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  int16_t *buffer = (int16_t *)malloc(config->audio_buffer_size * sizeof(int16_t));
  if (buffer == NULL) {
    return SL_STATUS_ALLOCATION_FAILED;
  }

  const sl_status_t status =
      sl_ml_audio_feature_generation_init_with_buffer(buffer, (int)config->audio_buffer_size, config);
  if (status != SL_STATUS_OK) {
    free(buffer);
    return status;
  }
  sli_ml_fe_si91x_owned_heap_buffer = buffer;
  return SL_STATUS_OK;
}

sl_status_t sl_ml_audio_feature_generation_set_mic_callback(sl_ml_audio_feature_generation_mic_callback_t callback,
                                                            void *arg)
{
  mic_callback = callback;
  mic_callback_arg = arg;
  return SL_STATUS_OK;
}

extern "C" void sli_ml_audio_feature_generation_si91x_release(void)
{
  if (sli_fe_runtime_cfg_valid) {
    FrontendReset(&frontend_state);
  }

  if (feature_buffer != NULL) {
    free(feature_buffer);
    feature_buffer = NULL;
  }

  feature_buffer_size = 0;
  audio_buffer_size = 0;
  audio_buffer = NULL;
  audio_buffer_read_index = 0;
  audio_buffer_write_index = 0;
  feature_buffer_read_index = 0;
  feature_buffer_write_index = 0;
  num_unfetched_slices = 0;
  sli_fe_runtime_cfg_valid = false;
  quantize_dynamic_scale_range = 0;
}
