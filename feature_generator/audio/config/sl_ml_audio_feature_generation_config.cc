/***************************************************************************//**
 * @file sl_ml_audio_feature_generation_config.cc
 * @brief Legacy symbol definitions for upgraded aiml 3.0.0 audio applications.
 *******************************************************************************
 * SPDX-License-Identifier: Zlib
 *
 * Defines only the globals still referenced by application code after SDK
 * upgrade. New projects should use sl_fe_audio_<instance>_config.h instead.
 ******************************************************************************/

#include "ml/tflite_micro_model/tflite_model_parameters.hpp"
#include "ml/tflite_micro_model/tflite_micro_logger.hpp"
#include "sl_ml_audio_feature_generation_config.h"

#include <cstdio>

template<typename T>
bool get_param(const npu_toolkit::TfliteModelParameters& model_parameters, const char *key, T& val)
{
    if(!model_parameters.get(key, val))
    {
        printf("Model parameter missing: %s\n", key);
        val = 0;
        return true;
    }
    else
    {
        return false;
    }
}


template<typename T>
bool get_param_default(const npu_toolkit::TfliteModelParameters& model_parameters, const char *key, T& val, T default_val)
{
    if(!model_parameters.get(key, val))
    {
        val = default_val;
        return true;
    }
    return false;
}

#define GET_INT(key, val) missing_params |= get_param(model_parameters, key, val);  printf("%s=%d\n", key, val)
#define GET_INT_DEFAULT(key, val, default_val) get_param_default(model_parameters, key, val, default_val);  printf("%s=%d\n", key, val)
#define GET_FLOAT(key, val) missing_params |= get_param(model_parameters, key, val);  printf("%s=%5.3f\n", key, val)
#define GET_FLOAT_DEFAULT(key, val, default_val) get_param_default(model_parameters, key, val, default_val);  printf("%s=%5.3f\n", key, val)

int SL_ML_AUDIO_FEATURE_GENERATION_DUMP_RAW_SPECTROGRAM;
int SL_ML_AUDIO_FEATURE_GENERATION_DUMP_QUANTIZED_SPECTROGRAM;
int SL_ML_AUDIO_FEATURE_GENERATION_DUMP_AUDIO;
int SL_TFLITE_MODEL_AUDIO_FEATURE_GENERATION_AUDIO_GAIN;
int SL_TFLITE_MODEL_FE_ACTIVITY_DETECTION_ENABLE;

bool sl_ml_audio_feature_generation_load_model_settings(const void *tflite_flatbuffer)
{
  npu_toolkit::TfliteModelParameters model_parameters;
  bool missing_params = false;

  if (!npu_toolkit::TfliteModelParameters::load_from_tflite_flatbuffer(tflite_flatbuffer,
                                                                       model_parameters)) {
    return false;
  }

  printf("AudioFeatureGenerator settings:\n");
  GET_INT_DEFAULT("fe.activity_detection_enable", SL_TFLITE_MODEL_FE_ACTIVITY_DETECTION_ENABLE, 0);
  GET_INT("volume_gain", SL_TFLITE_MODEL_AUDIO_FEATURE_GENERATION_AUDIO_GAIN);

  return !missing_params;
}
