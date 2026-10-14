/***************************************************************************//**
 * @file
 * @brief image feature generation config
 *******************************************************************************
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

#include "tflite_model_parameters.hpp"
#include "tflite_micro_logger.hpp"
#include "sl_ml_image_feature_generation_config.h"

#include <cstdio>

// Same convention as audio config get_param / get_param_default: true if key is absent
// from the model (default applied), false if present.

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

#define GET_FLOAT_DEFAULT(key, val, default_val) get_param_default(model_parameters, key, val, default_val);  printf("%s=%5.3f\n", key, val)
#define GET_INT_DEFAULT(key, val, default_val) get_param_default(model_parameters, key, val, default_val);  printf("%s=%d\n", key, val)

extern "C"
{

float SL_TFLITE_MODEL_IMAGE_SAMPLEWISE_NORM_RESCALE;
int   SL_TFLITE_MODEL_IMAGE_SAMPLEWISE_NORM_MEAN_AND_STD;

bool sl_ml_image_feature_generation_load_model_settings(const void* tflite_flatbuffer)
{
    npu_toolkit::TfliteModelParameters model_parameters;

    if(!npu_toolkit::TfliteModelParameters::load_from_tflite_flatbuffer(tflite_flatbuffer, model_parameters))
    {
        return false;
    }

    printf("ImageFeatureGenerator settings:\n");
    GET_FLOAT_DEFAULT("samplewise_norm.rescale", SL_TFLITE_MODEL_IMAGE_SAMPLEWISE_NORM_RESCALE, 0.f);
    GET_INT_DEFAULT("samplewise_norm.mean_and_std", SL_TFLITE_MODEL_IMAGE_SAMPLEWISE_NORM_MEAN_AND_STD, 0);

    return true;
}

} // extern "C"
