/***************************************************************************//**
 * @file sl_ml_profiler_debug_channel.h
 * @brief Header to define structs for the Silabs ML Profiler debug channel.
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
#ifndef SL_ML_PROFILER_DEBUG_CHANNEL_H
#define SL_ML_PROFILER_DEBUG_CHANNEL_H

#include "sl_ml_profiler_config.h"
#include <stdint.h>
#include "sl_ml_processor_id.h"
#include "sl_ml_mcu_id.h"
#include "sl_ml_accelerator_id.h"
#include "sl_ml_counter_unit_id.h"
#include <stdio.h>
#include <stdbool.h>

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
// Track UUID parameters
#define SL_ML_TRACK_UUID_BYTES  8
#define COUNTER_PACKET_BUF_SIZE 64
#define START_TRACK_PACKET_BUF_SIZE 64
#define END_TRACK_PACKET_BUF_SIZE 64

#ifdef __cplusplus
extern "C" {
#endif

// Start session information
typedef struct {
    const char *model_name;
    const char *board_name;
    const char *opn;
    const char *part_family;
    uint16_t flash_kb;
    uint16_t ram_kb;
    uint16_t arena_kb;
    sl_ml_mcu_id_t mcu_id;
    sl_ml_accelerator_id_t accel_id;
} sl_ml_profiler_start_session_info_t;

// End session performance statistics
typedef struct {
    uint64_t total_num_cycles;        // count (uint64, LE)
    uint64_t total_num_stalls;        // count (uint64, LE)
    uint64_t total_acc_cycles;        // count (uint64, LE)
    uint64_t total_acc_stalls;        // count (uint64, LE)
    uint64_t total_num_ops;           // count (uint64, LE)
    uint64_t total_num_macs;          // count (uint64, LE)
    uint32_t total_flash_used_kb;     // size in KB (uint32, LE)
    uint32_t total_ram_used_kb;       // size in KB (uint32, LE)
    float    mean_mac_per_cycle;      // float (IEEE-754, f32, LE)
    float    mean_inference_time_ms;  // float (IEEE-754, f32, LE)
#ifdef SL_ML_ENABLE_ENERGY_PROFILING
    float    energy_joules;           // float (IEEE-754, f32, LE)
#endif // SL_ML_ENABLE_ENERGY_PROFILING
} sl_ml_profiler_end_session_info_t;

// shape information
typedef struct {
    uint8_t n_dimensions ;     // number of dimensions (e.g., 4 for [N,H,W,C])
    uint16_t shape[4];          // shape of layer input HxWxC (array of n_dimensions uint16_t)
} layer_dimension_t;

// Start event information
typedef struct {
    layer_dimension_t input_shape;          // shape of layer input
    layer_dimension_t output_shape;         // shape of layer output
    const char *function_name;         // name of the function (for debugging)
    uint16_t    line_number;           // line number in source code
    sl_ml_processor_id_t processor_id;         // processor identifier
    uint64_t num_cpu_cycles;      // count (uint64, LE)
    uint64_t num_cpu_stalls;      // count (uint64, LE)
    uint64_t num_mvp_cycles;      // count (uint64, LE)
    uint64_t num_mvp_stalls;      // count (uint64, LE)
    float    cpu_util_percent;    // float (IEEE-754, f32, LE)
    float    clock_rate_hz;       // float (IEEE-754, f32, LE)
    float    mac_per_cycle;       // float (IEEE-754, f32, LE)
#ifdef SL_ML_ENABLE_ENERGY_PROFILING
    float    energy_joules;       // float (IEEE-754, f32, LE)
#endif // SL_ML_ENABLE_ENERGY_PROFILING
    uint64_t macs;
    uint64_t ops;
} sl_ml_profiler_event_info_t;

#if (SL_ML_ENABLE_PROFILER_DEBUG_MSG)
// Start track information
typedef struct {
    sl_ml_processor_id_t processor_id;       /* MCU/accelerator identifier */
    sl_ml_counter_unit_id_t counter_unit;       /* Counter unit*/
    bool has_parent;
    bool is_counter;
} sl_ml_profiler_start_track_info_t;

// counter track information
typedef struct {
    double   counter_value;       /* Counter unit*/
    sl_ml_counter_unit_id_t counter_unit;       /* Counter unit identifier */
} sl_ml_profiler_counter_pkt_info_t;

// End track information
typedef struct {
    sl_ml_processor_id_t processor_id;       /* MCU/accelerator identifier */
    //sl_ml_counter_unit_id_t counter_unit;       /* Counter unit*/
    bool has_parent;
    bool is_counter;
} sl_ml_profiler_end_track_info_t;

void sl_ml_profiler_start_session_write(sl_ml_profiler_start_session_info_t *start_session_info);
void sl_ml_profiler_end_session_write(sl_ml_profiler_end_session_info_t *end_session_info);
void sl_ml_profiler_start_event_write(sl_ml_profiler_event_info_t *start_event_info);
void sl_ml_profiler_end_event_write(sl_ml_profiler_event_info_t *end_event_info);
void sl_ml_profiler_counter_start_track_write(sl_ml_profiler_start_track_info_t *start_track_info);
void sl_ml_profiler_core_start_track_write(sl_ml_profiler_start_track_info_t *start_track_info);
void sl_ml_profiler_counter_end_track_write(sl_ml_profiler_end_track_info_t *end_track_info);
void sl_ml_profiler_core_end_track_write(sl_ml_profiler_end_track_info_t *end_track_info);
void sl_ml_profiler_counter_pkt_write(sl_ml_profiler_counter_pkt_info_t *counter_info);
void sl_ml_profiler_debug_init();

#endif // SL_ML_ENABLE_PROFILER_DEBUG_MSG

#ifdef __cplusplus
}
#endif
#endif /* SL_ML_PROFILER_DEBUG_CHANNEL_H */


