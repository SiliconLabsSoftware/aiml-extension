/***************************************************************************//**
 * @file sl_ml_mcu_id.h
 * @brief Defines MCU IDs.
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
#ifndef SL_ML_MCU_ID_H
#define SL_ML_MCU_ID_H

#include <stdint.h>
#include "string.h"
/**
 * @brief Enumeration of supported MCU types.
 * 
 * Used for identifying the processing core on a part for the start session packet.
 */
typedef enum {
    SL_ML_MCU_ID_ARM_CORTEX_M33 = 0x02,
    SL_ML_MCU_ID_ARM_CORTEX_M4  = 0x03,
    SL_ML_MCU_ID_INVALID        = 0xFF,
} sl_ml_mcu_id_t;

static inline const char* map_mcu_id(sl_ml_mcu_id_t id) {
    switch (id) {
        case SL_ML_MCU_ID_ARM_CORTEX_M33: return "Cortex-M33";
        case SL_ML_MCU_ID_ARM_CORTEX_M4: return "Cortex-M4";
        default:    return "none";
    }
}

#endif /* SL_ML_MCU_ID_H */
