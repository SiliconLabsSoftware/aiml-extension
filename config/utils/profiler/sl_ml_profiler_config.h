/***************************************************************************//**
 * @file sl_ml_profiler_config.h
 * @brief Model profiler configuration.
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

 // <<< Use Configuration Wizard in Context Menu >>>

#ifndef SL_ML_PROFILER_CONFIG_H
#define SL_ML_PROFILER_CONFIG_H

/*******************************************************************************
 ******************************   DEFINES   ************************************
 ******************************************************************************/

// <o SL_NUMBER_OF_SAMPLES_TO_PROCESS> Number of frames/samples to process
// <i> Defines number of inference frames to process before ending the profiling session.  
// <i> Default: 1
#define SL_NUMBER_OF_SAMPLES_TO_PROCESS                     (1)

// <e SL_ML_ENABLE_DYNAMIC_MODEL_LOAD> Enables firmware to load input model dynamically
// <i> If this is enabled, profiler firmware loads model dynamically with fixed address location.
// <i> Make sure update flash memory address location based on model size in slcc file.
// <i> Default: 1
#define SL_ML_ENABLE_DYNAMIC_MODEL_LOAD                           (0)
// </e>

// <e SL_ML_ENABLE_PROFILER_DEBUG_MSG> Enables output debug messages and dynamic model
// <i> If this is enabled, profiler sends debug channel messages.
// <i> Default: 1
#define SL_ML_ENABLE_PROFILER_DEBUG_MSG                           (0)
// </e>

// <o PRIMARY_TENSOR_ARENA_SIZE_SI91X> Primary Tensor Arena Size for SI91X board
// <i> Provides initial buffer size for the primary tensor arena from BSS. 
// <i> This buffer is used for input, output and intermediate tensors.
// <i> Value set to maximum of 128KB.
// <i> Default: 131027
#define PRIMARY_TENSOR_ARENA_SIZE_SI91X                         (131027)

// <o MODEL_CACHE_SIZE_SI91X> Model Cache Size for SI91X board.
// <i> Provides initial buffer size for the model cache.
// <i> This buffer is used for operator data caching.
// <i> Value set to maximum of 32KB.
// <i> Default: 32768
#define MODEL_CACHE_SIZE_SI91X                                  (32768)

// <o SL_ML_PROFILER_DEBUG_MODEL_BASE_ADDR> Model base address for dynamic model load
// <i> Provides flash base address location for dynamic model load.
// <i> Default: 0x08000000
#define SL_ML_PROFILER_DEBUG_MODEL_BASE_ADDR                    (0x08000000)

#endif // SL_ML_PROFILER_CONFIG_H

// <<< end of configuration section >>>
