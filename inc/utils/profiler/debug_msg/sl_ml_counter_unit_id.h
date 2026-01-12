/***************************************************************************//**
 * @file sl_ml_counter_unit_id.h
 * @brief Defines counter unit IDs.
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
#ifndef SL_ML_COUNTER_UNIT_ID_H
#define SL_ML_COUNTER_UNIT_ID_H

#include "sl_ml_profiler_debug_channel.h"
#if (SL_ML_ENABLE_PROFILER_DEBUG_MSG)

#include <stdint.h>
#define SL_ML_COUNTER_UNIT_ID_LEN 3 // number of counter units
/**
 * @brief Enumeration of identifiers for Start Event Packet.
 */
typedef enum {
    SL_ML_COUNTER_UNIT_MHZ = 0x00,
    SL_ML_COUNTER_UNIT_JOULES = 0x01,
    SL_ML_COUNTER_UNIT_KB = 0x02
} sl_ml_counter_unit_id_t;


static inline int get_counter_unit_index(sl_ml_counter_unit_id_t id) {
    switch (id) {
        case SL_ML_COUNTER_UNIT_MHZ: return 0;
        case SL_ML_COUNTER_UNIT_JOULES: return 1;
        case SL_ML_COUNTER_UNIT_KB: return 2;
        default:    return -1;
    }
}
#endif //(SL_ML_ENABLE_PROFILER_DEBUG_MSG)
#endif /* SL_ML_COUNTER_UNIT_ID_H */
