/***************************************************************************//**
 * @file
 * @brief Configuration file for Audio Frontend
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
#ifndef SL_ML_AUDIO_FEATURE_GENERATION_CONFIG_H
#define SL_ML_AUDIO_FEATURE_GENERATION_CONFIG_H

#pragma message("sl_ml_audio_feature_generation_config.h is deprecated.\
  Please remove this file from your application, it will be removed in a future release.\
  It is replaced by instance specific header such as sl_fe_audio_<instance>_config.h, which is already in your project now.\
  <instance> is the instance name chosen when you added the fe_audio component.")

#include "sl_fe_audio_instances.h"

extern int SL_ML_AUDIO_FEATURE_GENERATION_DUMP_RAW_SPECTROGRAM;
extern int SL_ML_AUDIO_FEATURE_GENERATION_DUMP_QUANTIZED_SPECTROGRAM;
extern int SL_ML_AUDIO_FEATURE_GENERATION_DUMP_AUDIO;
extern int SL_TFLITE_MODEL_AUDIO_FEATURE_GENERATION_AUDIO_GAIN;
extern int SL_TFLITE_MODEL_FE_ACTIVITY_DETECTION_ENABLE;

#define SL_ML_AUDIO_FEATURE_GENERATION_AUDIO_GAIN        SL_TFLITE_MODEL_AUDIO_FEATURE_GENERATION_AUDIO_GAIN
#define SL_ML_FRONTEND_ACTIVITY_DETECTION_ENABLE       SL_TFLITE_MODEL_FE_ACTIVITY_DETECTION_ENABLE

#ifndef __cplusplus
#define sl_ml_audio_feature_generation_init() \
  sl_ml_audio_feature_generation_init(sl_fe_audio_get_default_config())
#define sl_ml_audio_feature_generation_frontend_init() \
  sl_ml_audio_feature_generation_frontend_init(sl_fe_audio_get_default_config())
#else

/***************************************************************************//**
 * @deprecated Use sl_ml_audio_feature_generation_init(&sl_fe_audio_<instance>_cfg) instead.
 ******************************************************************************/
[[deprecated("Use sl_ml_audio_feature_generation_init(&sl_fe_audio_<instance>_cfg) instead.")]]
inline sl_status_t sl_ml_audio_feature_generation_init(void)
{
  return sl_ml_audio_feature_generation_init(sl_fe_audio_get_default_config());
}

/***************************************************************************//**
 * @deprecated Use sl_ml_audio_feature_generation_frontend_init(config) instead.
 ******************************************************************************/
[[deprecated("Use sl_ml_audio_feature_generation_frontend_init(&sl_fe_audio_<instance>_cfg) instead.")]]
inline sl_status_t sl_ml_audio_feature_generation_frontend_init(void)
{
  return sl_ml_audio_feature_generation_frontend_init(sl_fe_audio_get_default_config());
}

#endif

/***************************************************************************//**
 * @deprecated Remove calls to this function; use SL_FE_* macros in
 *             sl_fe_audio_<instance>_config.h instead, and use
 *             sl_ml_audio_feature_generation_is_activity_detection_enabled() instead of
 *             SL_ML_FRONTEND_ACTIVITY_DETECTION_ENABLE for activity detection.
 ******************************************************************************/
__attribute__((deprecated("Remove calls to sl_ml_audio_feature_generation_load_model_settings(); use SL_FE_* macros in sl_fe_audio_<instance>_config.h instead, and use sl_ml_audio_feature_generation_is_activity_detection_enabled() instead of SL_ML_FRONTEND_ACTIVITY_DETECTION_ENABLE for activity detection.")))
bool sl_ml_audio_feature_generation_load_model_settings(const void* tflite_flatbuffer);
#endif // SL_ML_AUDIO_FEATURE_GENERATION_CONFIG_H
