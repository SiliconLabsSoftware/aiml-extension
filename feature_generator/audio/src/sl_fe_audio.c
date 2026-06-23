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

 #include "sl_status.h"
 #include "sl_mic.h"
 #include "microfrontend/lib/frontend.h"
 #include "microfrontend/lib/frontend_util.h"
 #include "sl_fe_audio.h"
 #include "sl_ml_audio_feature_generation_config.h"
 #include "sl_common.h"
 
 #ifndef SL_ML_AUDIO_FEATURE_GENERATION_QUANTIZE_DYNAMIC_SCALE_RANGE_DB
 #define SL_ML_AUDIO_FEATURE_GENERATION_QUANTIZE_DYNAMIC_SCALE_RANGE_DB 40
 #endif
 
 /*******************************************************************************
  *********************************   DEFINES   *********************************
  ******************************************************************************/
 #define AUDIO_BUFFER_CHUNK_SIZE     512
 #define MIC_DMA_SIZE                512

 /*******************************************************************************
  ***************************  LOCAL VARIABLES   ********************************
  ******************************************************************************/

 static int16_t mic_sample_buffer[2 * MIC_DMA_SIZE];
 
 static size_t audio_buffer_cap;
 static size_t feature_buffer_cap;
 
 static int16_t *audio_buffer;
 static uint16_t *feature_buffer;
 
 static volatile size_t audio_buffer_read_index;
 static volatile size_t audio_buffer_write_index;
 static volatile size_t feature_buffer_start;
 
 static size_t num_unfetched_slices = 0;
 
 static int audio_volume_scaler;
 
 struct FrontendState frontend_state;
 
 /*******************************************************************************
  **************************   LOCAL FUNCTIONS   ********************************
  ******************************************************************************/
 
 static sl_status_t process_audio_buffer_chunk(const int16_t *audio_data, size_t *slices_updated);
 
 /*******************************************************************************
  **************************   GLOBAL FUNCTIONS   *******************************
  ******************************************************************************/
 
 sl_status_t sl_ml_audio_feature_generation_frontend_init()
 {
   {
     const int sample_len = SL_ML_FRONTEND_SAMPLE_LENGTH_MS;
     const int win_size = SL_ML_FRONTEND_WINDOW_SIZE_MS;
     const int win_step = SL_ML_FRONTEND_WINDOW_STEP_MS;
     if (SL_ML_FRONTEND_SAMPLE_RATE_HZ <= 0 || win_step <= 0 || sample_len <= win_size) {
       return SL_STATUS_FAIL;
     }
   }
 
   if (audio_buffer != NULL) {
     free(audio_buffer);
     audio_buffer = NULL;
   }
   if (feature_buffer != NULL) {
     free(feature_buffer);
     feature_buffer = NULL;
   }
 
   /* Ring buffer capacity must be a multiple of AUDIO_BUFFER_CHUNK_SIZE: each mic DMA
    * callback and each frontend step uses 512 contiguous samples from audio_buffer[index].
    * If cap were e.g. 8000, index 7680 + 512 would exceed the allocation (write overflow;
    * same for read in process_audio_buffer_chunk). Round up so chunk-aligned indices wrap
    * within the buffer. */
   {
     const size_t raw_cap = (size_t)SL_ML_AUDIO_FEATURE_GENERATION_AUDIO_BUFFER_SIZE;
     const size_t chunk = (size_t)AUDIO_BUFFER_CHUNK_SIZE;
     if (raw_cap == 0U) {
       return SL_STATUS_FAIL;
     }
     audio_buffer_cap = ((raw_cap + chunk - 1U) / chunk) * chunk;
   }
   {
     int sample_len = SL_ML_FRONTEND_SAMPLE_LENGTH_MS;
     int win_size = SL_ML_FRONTEND_WINDOW_SIZE_MS;
     int win_step = SL_ML_FRONTEND_WINDOW_STEP_MS;
     size_t slice_count = (size_t)(1 + ((sample_len - win_size) / win_step));
     feature_buffer_cap = (size_t)SL_ML_FRONTEND_FILTERBANK_N_CHANNELS * slice_count;
   }
 
   audio_buffer = (int16_t *)malloc(audio_buffer_cap * sizeof(int16_t));
   feature_buffer = (uint16_t *)malloc(feature_buffer_cap * sizeof(uint16_t));
   if ((audio_buffer == NULL) || (feature_buffer == NULL)) {
     return SL_STATUS_ALLOCATION_FAILED;
   }
 
   audio_volume_scaler = SL_ML_AUDIO_FEATURE_GENERATION_AUDIO_GAIN;
 
   for (size_t i = 0; i < audio_buffer_cap; i++) {
     audio_buffer[i] = 0;
   }
   for (size_t i = 0; i < feature_buffer_cap; i++) {
     feature_buffer[i] = 0;
   }
 
   audio_buffer_read_index = 0;
   audio_buffer_write_index = 0;
   feature_buffer_start = 0;
 
   struct FrontendConfig config;
   // ========    init  configurations  ========
   FrontendFillConfigWithDefaults(&config);
   // ========    init configurations  ========
   config.window.size_ms = SL_ML_FRONTEND_WINDOW_SIZE_MS;
   config.window.step_size_ms = SL_ML_FRONTEND_WINDOW_STEP_MS;
 
   config.filterbank.num_channels = SL_ML_FRONTEND_FILTERBANK_N_CHANNELS;
   config.filterbank.lower_band_limit = SL_ML_FRONTEND_FILTERBANK_LOWER_BAND_LIMIT;
   config.filterbank.upper_band_limit = SL_ML_FRONTEND_FILTERBANK_UPPER_BAND_LIMIT;
   config.filterbank.output_scale_shift = 0;
 
   config.noise_reduction.enable_noise_reduction = SL_ML_FRONTEND_NOISE_REDUCTION_ENABLE;
   config.noise_reduction.smoothing_bits = SL_ML_FRONTEND_NOISE_REDUCTION_SMOOTHING_BITS;
   config.noise_reduction.even_smoothing = SL_ML_FRONTEND_NOISE_REDUCTION_EVEN_SMOOTHING;
   config.noise_reduction.odd_smoothing = SL_ML_FRONTEND_NOISE_REDUCTION_ODD_SMOOTHING;
   config.noise_reduction.min_signal_remaining = SL_ML_FRONTEND_NOISE_REDUCTION_MIN_SIGNAL_REMAINING;
 
   config.pcan_gain_control.enable_pcan = SL_ML_FRONTEND_PCAN_ENABLE;
   config.pcan_gain_control.strength = SL_ML_FRONTEND_PCAN_STRENGTH;
   config.pcan_gain_control.offset = SL_ML_FRONTEND_PCAN_OFFSET;
   config.pcan_gain_control.gain_bits = SL_ML_FRONTEND_PCAN_GAIN_BITS;
 
   config.log_scale.enable_log = SL_ML_FRONTEND_LOG_SCALE_ENABLE;
   config.log_scale.scale_shift = SL_ML_FRONTEND_LOG_SCALE_SHIFT;
   // ========    additional configurations ========
   config.activity_detection.enable_activation_detection = SL_ML_FRONTEND_ACTIVITY_DETECTION_ENABLE;
   config.activity_detection.alpha_a = SL_ML_FRONTEND_ACTIVITY_DETECTION_ALPHA_A;
   config.activity_detection.alpha_b = SL_ML_FRONTEND_ACTIVITY_DETECTION_ALPHA_B;
   config.activity_detection.arm_threshold = SL_ML_FRONTEND_ACTIVITY_DETECTION_ARM_THRESHOLD;
   config.activity_detection.trip_threshold = SL_ML_FRONTEND_ACTIVITY_DETECTION_TRIP_THRESHOLD;
 
   config.dc_notch_filter.enable_dc_notch_filter = SL_ML_FRONTEND_DC_NOTCH_FILTER_ENABLE;
   config.dc_notch_filter.coefficient = SL_ML_FRONTEND_DC_NOTCH_FILTER_COEFFICIENT;
   // ========    additional configurations  ========
 
   if (!FrontendPopulateState(&config, &frontend_state, SL_ML_FRONTEND_SAMPLE_RATE_HZ)) {
     return SL_STATUS_FAIL;
   }
 
   return SL_STATUS_OK;
 }
 
 sl_status_t sli_ml_audio_feature_generation_audio_buffer_write_chunk(const int16_t *new_samples, size_t num_samples)
 {
   if (num_samples != AUDIO_BUFFER_CHUNK_SIZE) {
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
 
   while (audio_buffer_read_index != audio_buffer_write_index) {
     process_audio_buffer_chunk(&audio_buffer[audio_buffer_read_index], &num_slices_updated);
     audio_buffer_read_index = (audio_buffer_read_index + AUDIO_BUFFER_CHUNK_SIZE) % audio_buffer_cap;
   }
 
   if (num_slices_updated == 0) {
     return SL_STATUS_EMPTY;
   }
 
   num_unfetched_slices += num_slices_updated;
   return SL_STATUS_OK;
 }
 
 sl_status_t sl_ml_audio_feature_generation_get_features_raw(uint16_t *buffer, size_t num_elements)
 {
   if (num_elements != feature_buffer_cap) {
     return SL_STATUS_INVALID_PARAMETER;
   }
 
   for (size_t i = 0; i < feature_buffer_cap; i++) {
     const uint32_t capture_index = (uint32_t)((feature_buffer_start + i) % feature_buffer_cap);
     buffer[i] = feature_buffer[capture_index];
   }
 
   num_unfetched_slices = 0;
   return SL_STATUS_OK;
 }
 
 sl_status_t sli_ml_audio_feature_generation_get_features_quantized(int8_t *buffer,
                                                                  size_t num_elements,
                                                                  uint16_t range_min,
                                                                  uint16_t range_max)
 {
   if (num_elements != feature_buffer_cap) {
     return SL_STATUS_INVALID_PARAMETER;
   }
 
   if (range_min == range_max) {
     return SL_STATUS_INVALID_PARAMETER;
   }
 
   for (size_t i = 0; i < feature_buffer_cap; i++) {
     const int capture_index = (int)((feature_buffer_start + i) % feature_buffer_cap);
     const int32_t value_scale = 256;
     const int32_t value_div = range_max - range_min;
     int32_t value = (((feature_buffer[capture_index] - range_min) * value_scale) + (value_div / 2))
                     / value_div;
     value -= 128;
     if (value < -128) {
       value = -128;
     }
     if (value > 127) {
       value = 127;
     }
     buffer[i] = (int8_t)value;
   }
 
   num_unfetched_slices = 0;
   return SL_STATUS_OK;
 }
 
 sl_status_t sli_ml_audio_feature_generation_get_features_dynamically_quantized(int8_t *buffer,
                                                                                size_t num_elements,
                                                                                uint16_t dynamic_range)
 {
   if (num_elements != feature_buffer_cap) {
     return SL_STATUS_INVALID_PARAMETER;
   }
 
   if (dynamic_range == 0) {
     return SL_STATUS_INVALID_PARAMETER;
   }
 
   int32_t maxval = 0;
   for (size_t i = 0; i < feature_buffer_cap; i++) {
     const int capture_index = (int)((feature_buffer_start + i) % feature_buffer_cap);
     const int32_t value = (int32_t)feature_buffer[capture_index];
     maxval = SL_MAX(value, maxval);
   }
 
   const int32_t minval = SL_MAX(maxval - (int32_t)dynamic_range, 0);
   const int32_t val_range = SL_MAX(maxval - minval, 1);
 
   for (size_t i = 0; i < feature_buffer_cap; i++) {
     const int capture_index = (int)((feature_buffer_start + i) % feature_buffer_cap);
     int32_t value = (int32_t)feature_buffer[capture_index];
     value -= minval;
     value *= 255;
     value /= val_range;
     value -= 128;
     value = SL_MIN(SL_MAX(value, -128), 127);
     buffer[i] = (int8_t)value;
   }
 
   num_unfetched_slices = 0;
   return SL_STATUS_OK;
 }
 
 sl_status_t sl_ml_audio_feature_generation_fill_tensor(TfLiteTensor *input_tensor)
 {
   sl_status_t status = SL_STATUS_OK;
   if (input_tensor->type == kTfLiteInt8) {
 
     if (SL_ML_AUDIO_FEATURE_GENERATION_QUANTIZE_DYNAMIC_SCALE_ENABLE) {
 #define LN10_DIV_20 0.11512925465f
       const int dynamic_scale_range = (int)(SL_ML_AUDIO_FEATURE_GENERATION_QUANTIZE_DYNAMIC_SCALE_RANGE_DB
                                             * (float)(1 << SL_ML_FRONTEND_LOG_SCALE_SHIFT) * LN10_DIV_20);
       status = sli_ml_audio_feature_generation_get_features_dynamically_quantized(input_tensor->data.int8,
                                                                                 input_tensor->bytes,
                                                                                 (uint16_t)dynamic_scale_range);
 #undef LN10_DIV_20
     } else {
       status = sli_ml_audio_feature_generation_get_features_quantized(input_tensor->data.int8,
                                                                       input_tensor->bytes,
                                                                       SL_ML_AUDIO_FEATURE_GENERATION_QUANTIZE_FEATURE_RANGE_MIN,
                                                                       SL_ML_AUDIO_FEATURE_GENERATION_QUANTIZE_FEATURE_RANGE_MAX);
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
   if (feature_buffer != NULL) {
     for (size_t i = 0; i < feature_buffer_cap; i++) {
       feature_buffer[i] = 0;
     }
   }
   if (audio_buffer != NULL) {
     for (size_t i = 0; i < audio_buffer_cap; i++) {
       audio_buffer[i] = 0;
     }
   }
 
   audio_buffer_read_index = 0;
   audio_buffer_write_index = 0;
   feature_buffer_start = 0;
 
   num_unfetched_slices = 0;
 
   FrontendReset(&frontend_state);
 }
 
 int sl_ml_audio_feature_generation_get_feature_buffer_size()
 {
   return (int)feature_buffer_cap;
 }
 
 static sl_status_t process_audio_buffer_chunk(const int16_t *audio_data, size_t *slices_updated)
 {
   size_t num_samples = AUDIO_BUFFER_CHUNK_SIZE;
 
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
 sl_status_t sl_ml_audio_feature_generation_init()
 {
   sl_status_t mic_status;

 #if (SL_ML_AUDIO_FEATURE_GENERATION_MANUAL_CONFIG_ENABLE == 0)
   if (SL_TFLITE_MODEL_FE_SAMPLE_RATE_HZ <= 0 || SL_TFLITE_MODEL_FE_WINDOW_STEP_MS <= 0
       || SL_TFLITE_MODEL_FE_SAMPLE_LENGTH_MS <= SL_TFLITE_MODEL_FE_WINDOW_SIZE_MS) {
     return SL_STATUS_FAIL;
   }
 #endif

   mic_status = sl_mic_init(SL_ML_FRONTEND_SAMPLE_RATE_HZ, 1);
   if (mic_status != SL_STATUS_OK) {
     return mic_status;
   }

   mic_status = sl_mic_start_streaming(mic_sample_buffer,
                                       MIC_DMA_SIZE,
                                       (sl_mic_buffer_ready_callback_t)mic_buffer_ready_callback);

   if (mic_status != SL_STATUS_OK) {
     sl_mic_deinit();
     return mic_status;
   }

   mic_status = sl_ml_audio_feature_generation_frontend_init();
   if (mic_status != SL_STATUS_OK) {
     sl_mic_deinit();
     return mic_status;
   }

   return SL_STATUS_OK;
 }