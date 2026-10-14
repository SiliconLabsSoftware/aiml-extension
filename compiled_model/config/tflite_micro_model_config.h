/***************************************************************************//**
 * @file tflite_micro_model_config.h
 * @brief Configuration file for TensorFlow Lite Micro Model.
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

// <<< Use Configuration Wizard in Context Menu >>>

#ifndef TFLITE_MICRO_MODEL_CONFIG_H
#define TFLITE_MICRO_MODEL_CONFIG_H

/*******************************************************************************
 ******************************   DEFINES   ************************************
 ******************************************************************************/

// <e TFLITE_MICRO_LOG_LEVEL> TF-Lite Micro Logging Level
// <i> This specifies the logging level for TF-Lite Micro.
// <i> 0 - Enable all logs
// <i> 1 - Enable info,warn,error logs (debug logs disabled)
// <i> 2 - Enable warn,error logs (debug,info logs disabled)
// <i> 3 - Enable error logs (debug,info,warn logs disabled)
// <i> 4 - Disable all logs
// <i> If a given log level is disabled, then its corresponding
// <i> log macro is disabled which reduces code space.
// <i> Default: 4 (all logs disabled)
#define TFLITE_MICRO_LOG_LEVEL    4
// </e>


// If the model profiler is not used and TF-Lite Micro logs are disabled,
// then strip all error strings from TF-Lite Micro. This reduces code space.
#if !defined(SL_ML_ENABLE_SILABS_PROFILER) && TFLITE_MICRO_LOG_LEVEL >= 4
#define TF_LITE_STRIP_ERROR_STRINGS
#endif


#endif // TFLITE_MICRO_MODEL_CONFIG_H

// <<< end of configuration section >>>
