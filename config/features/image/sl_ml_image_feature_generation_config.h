/***************************************************************************//**
 * @file
 * @brief Silicon Labs Arducam camera peripheral driver
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef SL_ML_IMAGE_FEATURE_GENERATION_CONFIG_H
#define SL_ML_IMAGE_FEATURE_GENERATION_CONFIG_H


// <q SL_ML_IMAGE_FEATURE_GENERATION_MANUAL_CONFIG_ENABLE> Enable Manual Frontend Configurations
// <i> Enable manual configuration of the Frontend
// <i> This may overwrite settings that are already embedded in
// <i> a .tflite file.
// <i> Default: 0
#define SL_ML_IMAGE_FEATURE_GENERATION_MANUAL_CONFIG_ENABLE            0

// <<< Use Configuration Wizard in Context Menu >>>
#if (SL_ML_IMAGE_FEATURE_GENERATION_MANUAL_CONFIG_ENABLE == 0)
#include "sl_tflite_micro_model_parameters.h"

#define SL_ML_IMAGE_FEATURE_GENERATION_SAMPLEWISE_NORM_RESCALE          SL_TFLITE_MODEL_SAMPLEWISE_NORM_RESCALE
#define SL_ML_IMAGE_FEATURE_GENERATION_SAMPLEWISE_NORM_MEAN_AND_STD     SL_TFLITE_MODEL_SAMPLEWISE_NORM_MEAN_AND_STD
#else
#define SL_ML_IMAGE_FEATURE_GENERATION_SAMPLEWISE_NORM_RESCALE          0.0f
#define SL_ML_IMAGE_FEATURE_GENERATION_SAMPLEWISE_NORM_MEAN_AND_STD     True
#endif
/*******************************************************************************
 *******************   MANUAL CONFIGURATIONS   ************************
 ******************************************************************************/

// <o> Ping pong buffer count
// <i> Default: 2
#define SL_ML_IMAGE_BUFFER_COUNT                                      2

// <o> Default Image width
// <i> Size of image width in pixels
// <i> Default: 160
#define SL_ML_CAM_IMAGE_WIDTH                                        160

// <o> Default Image height
// <i> Size of image height in pixels
// <i> Default: 120
#define SL_ML_CAM_IMAGE_HEIGHT                                        120

// <o> Default model input width for model input
// <i> Size of image width in pixels
// <i> Default: 160
#define SL_ML_MODEL_INPUT_WIDTH                                        160

// <o> Default model input height for model input
// <i> Size of image height in pixels
// <i> Default: 120
#define SL_ML_MODEL_INPUT_HEIGHT                                        120

// <o> Default threshold for image mean
// <i> If image mean is < threshold skip detection
// <i> Default: 0
#define SL_ML_IMAGE_MEAN_THRESHOLD                                        0

#endif // SL_ML_IMAGE_FEATURE_GENERATION_CONFIG_H
