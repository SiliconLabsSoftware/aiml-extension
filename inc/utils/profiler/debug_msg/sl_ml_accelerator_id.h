/***************************************************************************//**
 * @file sl_ml_accelerator_id.h
 * @brief Header defines Accelerator IDs.
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
#ifndef SL_ML_ACCELERATOR_ID_H
#define SL_ML_ACCELERATOR_ID_H

#include <stdint.h>
#include "string.h"


/**
 * @brief Enumeration of supported Accelerator types.
 * 
 * Used for identifying the accelerator core on a part for the start session packet.
 */
typedef enum {
    SL_ML_ACCELERATOR_ID_MVPv1 = 0x00,
    SL_ML_ACCELERATOR_ID_INVALID = 0xFF,
} sl_ml_accelerator_id_t;

static inline const char* map_accelerator_id(sl_ml_accelerator_id_t id) {
    switch (id) {
        case SL_ML_ACCELERATOR_ID_MVPv1: return "mvpv1";
        default:    return "none";
    }
}

#endif /* SL_ML_ACCELERATOR_ID_H */
