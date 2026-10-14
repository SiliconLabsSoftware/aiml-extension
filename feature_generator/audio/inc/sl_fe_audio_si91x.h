/***************************************************************************//**
 * @file sl_fe_audio_si91x.h
 * @brief Audio feature generator API's for Si91x (fe_audio component)
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
 ******************************************************************************/

#ifndef SL_FE_AUDIO_SI91X_H
#define SL_FE_AUDIO_SI91X_H

#include "sl_status.h"
#include "sl_component_catalog.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#if defined(SL_CATALOG_TENSORFLOW_LITE_MICRO_PRESENT)
#include "micro_common.h"
#endif

#include "sl_fe_audio.h"

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @addtogroup fe_audio
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * @name SiWx917 additions
 *
 * API available only on SiWx917, declared in @c sl_fe_audio_si91x.h: caller-provided audio
 * buffers, additional feature getters, activity detection, and a microphone PCM callback.
 * On SiWx917 the audio ring buffer is heap-allocated from @c config->audio_buffer_size
 * unless the caller supplies one through @ref sl_ml_audio_feature_generation_init_with_buffer.
 *
 * ### Single pipeline (multi-instance configs)
 * The fe_audio component may define several @c sl_fe_audio_<instance>_cfg symbols (one per
 * model), but only one pipeline may be active: this implementation uses one set of global
 * buffers and one @c FrontendState at a time. After validating the new parameters,
 * @ref sl_ml_audio_feature_generation_init and
 * @ref sl_ml_audio_feature_generation_init_with_buffer tear down an existing pipeline
 * (same as @ref sl_ml_audio_feature_generation_deinit) and then bring up the new config, so
 * callers can switch models with a single @c init call.
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * @brief
 *    Initialize audio feature generation with a user-provided buffer.
 *
 * @details
 *    If a pipeline is already active, it is torn down first (same as
 *    @ref sl_ml_audio_feature_generation_deinit). A non-OK status from that step is the
 *    return value from @c sl_ml_mic_deinit() during deinit.
 *
 *    After the frontend is initialized, failures from @c sl_ml_mic_init,
 *    @c sl_ml_mic_start_streaming, or @c sl_ml_mic_process_action return that driver
 *    @c sl_status_t after cleaning up allocated resources.
 *
 * @param[in] buffer
 *    Pointer to the audio buffer to use for feature generation.
 *
 * @param[in] n_frames
 *    Number of int16 audio samples in the buffer; must equal @c config->audio_buffer_size.
 *
 * @param[in] config
 *    Per-model frontend and buffer parameters; must not be NULL.
 *
 * @retval SL_STATUS_OK Success.
 * @retval SL_STATUS_INVALID_PARAMETER @p config or @p buffer is NULL, @p n_frames is less than 2,
 *    @p n_frames does not equal @c config->audio_buffer_size, or invalid frontend parameters
 *    from @ref sl_ml_audio_feature_generation_frontend_init.
 * @retval SL_STATUS_FAIL Coarse validation failed (sample rate, window step/size, segment length),
 *    or microfrontend state cannot be populated.
 * @retval SL_STATUS_ALLOCATION_FAILED The feature ring buffer could not be allocated.
 *
 * @return
 *    Other non-OK @c sl_status_t values may be returned from the microphone / I2S driver
 *    during teardown or bring-up.
 ******************************************************************************/
sl_status_t sl_ml_audio_feature_generation_init_with_buffer(int16_t *buffer,
                                                              int n_frames,
                                                              const sl_fe_audio_config_t *config);

/***************************************************************************//**
 * @brief
 *    Copy the active spectrogram ring to a @c float32 buffer.
 *
 * @param[out] buffer
 *    Pointer to the buffer to store the feature data.
 *
 * @param[in] num_elements
 *    Must equal the active feature element count (@ref sl_ml_audio_feature_generation_get_feature_buffer_size).
 *
 * @note
 *    Overwrites the entire @p buffer.
 *
 * @retval SL_STATUS_OK Success.
 * @retval SL_STATUS_NOT_INITIALIZED Feature buffer is not allocated.
 * @retval SL_STATUS_INVALID_PARAMETER @p num_elements does not match the active feature buffer size.
 ******************************************************************************/
sl_status_t sl_ml_audio_feature_generation_get_features_raw_float32(float *buffer, size_t num_elements);

/***************************************************************************//**
 * @brief
 *    Copy the active spectrogram to @p buffer with each sample multiplied by @p scaler.
 *
 * @details
 *    Computes @c buffer[i] = (float)feature[i] * @p scaler for each element in the ring.
 *
 * @param[out] buffer
 *    Pointer to the buffer to store the scaled feature data.
 *
 * @param[in] num_elements
 *    Must equal the active feature element count (@ref sl_ml_audio_feature_generation_get_feature_buffer_size).
 *
 * @param[in] scaler
 *    Scaling factor applied to each feature value (must be non-zero).
 *
 * @note
 *    Overwrites the entire @p buffer.
 *
 * @retval SL_STATUS_OK Success.
 * @retval SL_STATUS_NOT_INITIALIZED Feature buffer is not allocated.
 * @retval SL_STATUS_INVALID_PARAMETER @p num_elements does not match the active feature buffer size,
 *    or @p scaler is zero.
 ******************************************************************************/
sl_status_t sl_ml_audio_feature_generation_get_features_scaled(float *buffer, size_t num_elements, float scaler);

/***************************************************************************//**
 * @brief
 *    Mean- and standard-deviation-normalized copy of the active spectrogram into @p buffer.
 *
 * @details
 *    For each element: @c (float(value) - mean) / std. Near-constant input yields zeros in @p buffer.
 *
 * @param[out] buffer
 *    Pointer to the buffer to store the normalized feature data.
 *
 * @param[in] num_elements
 *    Must equal the active feature element count (@ref sl_ml_audio_feature_generation_get_feature_buffer_size).
 *
 * @note
 *    Overwrites the entire @p buffer.
 *
 * @retval SL_STATUS_OK Success.
 * @retval SL_STATUS_NOT_INITIALIZED Feature buffer is not allocated.
 * @retval SL_STATUS_INVALID_PARAMETER @p num_elements does not match the active feature buffer size.
 ******************************************************************************/
sl_status_t sl_ml_audio_feature_generation_get_features_mean_std_normalized(float *buffer, size_t num_elements);

/***************************************************************************//**
 * @brief
 *    Query whether the activity-detection block has tripped since the last successful read.
 *
 * @details
 *    Call @ref sl_ml_audio_feature_generation_update_features periodically so the detector
 *    advances on new audio.
 *
 * @note
 *    Activity detection must be enabled in the active @c sl_fe_audio_config_t (see
 *    @ref sl_ml_audio_feature_generation_is_activity_detection_enabled).
 *
 * @note
 *    Internal trip state is cleared when this returns @c SL_STATUS_OK; a subsequent call returns
 *    @c SL_STATUS_IN_PROGRESS until new activity is detected.
 *
 * @retval SL_STATUS_OK Activity was detected since the last successful trip.
 * @retval SL_STATUS_IN_PROGRESS Activity detection is enabled but no new trip occurred.
 * @retval SL_STATUS_NOT_AVAILABLE Activity detection is not enabled in the active configuration.
 ******************************************************************************/
sl_status_t sl_ml_audio_feature_generation_activity_detected();

/***************************************************************************//**
 * @brief
 *    Return whether the runtime frontend config has activity detection enabled.
 *
 * @details
 *    After @ref sl_ml_audio_feature_generation_init, reflects the active @c sl_fe_audio_config_t.
 ******************************************************************************/
bool sl_ml_audio_feature_generation_is_activity_detection_enabled(void);

/***************************************************************************//**
 * @brief
 *    Microphone callback function type for audio feature generation.
 *
 * @param[in] arg      User-defined argument passed to the callback.
 * @param[in] data     Pointer to the audio data buffer.
 * @param[in] n_frames Number of audio frames in the buffer.
 ******************************************************************************/
typedef void (*sl_ml_audio_feature_generation_mic_callback_t)(void* arg, const int16_t* data, uint32_t n_frames);

/***************************************************************************//**
 * @brief
 *    Register the optional microphone PCM callback (or clear it).
 *
 * @param[in] callback  Function pointer (may be NULL to clear).
 * @param[in] arg       User argument passed to @p callback.
 *
 * @retval SL_STATUS_OK Always (registration cannot fail in the current implementation).
 ******************************************************************************/
sl_status_t sl_ml_audio_feature_generation_set_mic_callback(sl_ml_audio_feature_generation_mic_callback_t callback, void* arg);

/***************************************************************************//**
 * @brief
 *    External audio buffer for audio feature generation (mic / DMA ring backing store).
 ******************************************************************************/
extern int16_t* sl_ml_audio_feature_generation_audio_buffer;

/** @} (end name SiWx917 additions) */

#ifdef __cplusplus
}
#endif

/** @} (end addtogroup fe_audio) */

#endif /* SL_FE_AUDIO_SI91X_H */
