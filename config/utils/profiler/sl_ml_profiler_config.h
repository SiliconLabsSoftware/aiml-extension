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
// <i> Default: 0
#define SL_ML_ENABLE_DYNAMIC_MODEL_LOAD                           (0)
// </e>

// <e SL_ML_ENABLE_DYNAMIC_BUFFER_ALLOCATION> Allocate model runtime buffers from the heap
// <i> If this is enabled, profiler firmware reads buffer count and sizes from the
// <i> compiled memory plan and mallocs those buffers. If disabled, BSS arenas sized
// <i> by SL_ML_PROFILER_PRIMARY_TENSOR_ARENA_SIZE and SL_ML_PROFILER_MODEL_PAGING_BUFFER_SIZE are used.
// <i> Default: 0
#define SL_ML_ENABLE_DYNAMIC_BUFFER_ALLOCATION                    (1)
// </e>

// <e SL_ML_ENABLE_PROFILER_DEBUG_MSG> Enables output debug messages and dynamic model
// <i> If this is enabled, profiler sends debug channel messages.
// <i> Default: 0
#define SL_ML_ENABLE_PROFILER_DEBUG_MSG                           (0)
// </e>

// <o SL_ML_PROFILER_PRIMARY_TENSOR_ARENA_SIZE> Primary Tensor Arena Size for SI91X board
// <i> Static BSS primary tensor arena when `SL_ML_ENABLE_DYNAMIC_BUFFER_ALLOCATION` is 0.
// <i> Also used as the heap fallback size when a compiled memory plan is missing.
// <i> Value set to maximum of 144KB.
// <i> Default: 147456
#define SL_ML_PROFILER_PRIMARY_TENSOR_ARENA_SIZE                         (147456)

// <o SL_ML_PROFILER_MODEL_PAGING_BUFFER_SIZE> Model Cache Size for SI91X board.
// <i> Static BSS paging buffer when `SL_ML_ENABLE_DYNAMIC_BUFFER_ALLOCATION` is 0.
// <i> Value set to maximum of 32KB.
// <i> Default: 32768
#define SL_ML_PROFILER_MODEL_PAGING_BUFFER_SIZE                                  (32768)

// <o SL_ML_PROFILER_MODEL_BUFFER_HEADROOM_SIZE> Extra bytes added to the primary runtime buffer
// <i> Used when `SL_ML_ENABLE_DYNAMIC_BUFFER_ALLOCATION` is 1. The compiled memory plan sizes
// <i> only the model buffers; the allocator instance and TFLM persistent allocations also come
// <i> out of the first region. The value is doubled and retried when a load runs out of room.
// <i> Default: 4096
#define SL_ML_PROFILER_MODEL_BUFFER_HEADROOM_SIZE                (4096)

// <o SL_ML_PROFILER_DEBUG_MODEL_BASE_ADDR> Model base address for dynamic model load
// <i> Provides flash base address location for dynamic model load.
// <i> Default: 0x08000000
#define SL_ML_PROFILER_DEBUG_MODEL_BASE_ADDR                    (0x08000000)

#endif // SL_ML_PROFILER_CONFIG_H

// <<< end of configuration section >>>
