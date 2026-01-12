/***************************************************************************//**
 * @file sl_ml_processor_id.h
 * @brief Header defines processor IDs.
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
#ifndef SL_ML_PROCESSOR_ID_H
#define SL_ML_PROCESSOR_ID_H

#include <stdint.h>
#include "string.h"

#include "sl_ml_accelerator_id.h"
#include "sl_ml_mcu_id.h"

#define SL_ML_PROCESSOR_ID_LEN 4 // number of processor types
/**
 * @brief Enumeration of supported MCU/accelerator types.
 *
 * Used for identifying the processing core associated with a track in pftrace.
 */
typedef enum {
    PROCESSOR_TRACK_MVP                 = 0x00,  // processor ID for MVP
    PROCESSOR_TRACK_ARM_CORTEX_M33      = 0x02,  // processor ID for Cortex-M33
    PROCESSOR_TRACK_ARM_CORTEX_M4       = 0x03,  // processor ID for Cortex-M4
    PARENT_TRACK_PERFORMANCE_COUNTER    = 0x05,  // Parent track for performance counters
    PARENT_TRACK_PROCESSOR_CORES        = 0x06,  // Parent track for processor cores
    COUNTER_TRACK_CPU_FREQ              = 0x07,  // CPU frequency counter track
#ifdef SL_ML_ENABLE_ENERGY_PROFILING
    COUNTER_TRACK_ENERGY                = 0x08,  // Energy counter track
#endif // SL_ML_ENABLE_ENERGY_PROFILING
    COUNTER_TRACK_RAM_USAGE             = 0x09,  // RAM usage counter track
    INVALID_TRACK                       = 0xFF
} sl_ml_processor_id_t;


static inline int get_processor_id_index(sl_ml_processor_id_t id) {
    switch (id) {
        case PROCESSOR_TRACK_MVP: return 0;
        case PROCESSOR_TRACK_ARM_CORTEX_M33: return 2;
        case PROCESSOR_TRACK_ARM_CORTEX_M4:  return 3;
        default:    return -1;
    }
}

static inline int get_counter_id_index(sl_ml_processor_id_t id) {
    switch (id) {
        case COUNTER_TRACK_CPU_FREQ: return 0;
#ifdef SL_ML_ENABLE_ENERGY_PROFILING
        case COUNTER_TRACK_ENERGY: return 1;
#endif // SL_ML_ENABLE_ENERGY_PROFILING
        case COUNTER_TRACK_RAM_USAGE: return 2;
        default:    return -1;
    }
}

static inline sl_ml_processor_id_t map_mcu_to_processor_id(sl_ml_mcu_id_t id) {
    switch (id) {
        case SL_ML_MCU_ID_ARM_CORTEX_M33: return PROCESSOR_TRACK_ARM_CORTEX_M33;
        case SL_ML_MCU_ID_ARM_CORTEX_M4: return PROCESSOR_TRACK_ARM_CORTEX_M4;
        default:    return INVALID_TRACK;
    }
}

static inline sl_ml_processor_id_t map_accelerator_to_processor_id(sl_ml_accelerator_id_t id) {
    switch (id) {
        case SL_ML_ACCELERATOR_ID_MVPv1: return PROCESSOR_TRACK_MVP;
        default:    return INVALID_TRACK;
    }
}

static inline const char* map_processor_id(sl_ml_processor_id_t id) {
    switch (id) {
        case PROCESSOR_TRACK_MVP: return "mvpv1";
        case PROCESSOR_TRACK_ARM_CORTEX_M33: return "Cortex-M33";
        case PROCESSOR_TRACK_ARM_CORTEX_M4: return "Cortex-M4";
        default:    return "Unknown";
    }
}

#endif /* SL_ML_PROCESSOR_ID_H */
