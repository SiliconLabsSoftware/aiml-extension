/***************************************************************************//**
 * @file sl_ml_profiler_msg_id.h
 * @brief Defines message id for packets.
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

#ifndef SL_ML_PROFILER_MSG_ID_H
#define SL_ML_PROFILER_MSG_ID_H
#include "sl_ml_profiler_debug_channel.h"
#if (SL_ML_ENABLE_PROFILER_DEBUG_MSG)
#include <stdint.h>

/* Enumeration of all Profiler Message IDs used in Silicon Labs ML debug packets */
typedef enum {
    SL_ML_MSG_ID_START_SESSION = 0x00,  /* Start Session Packet */
    SL_ML_MSG_ID_START_TRACK   = 0x01,  /* Start Track Packet */
    SL_ML_MSG_ID_END_TRACK     = 0x02,  /* End Track Packet */
    SL_ML_MSG_ID_START_EVENT   = 0x03,  /* Start Event Packet */
    SL_ML_MSG_ID_END_EVENT     = 0x04,  /* End Event Packet */
    SL_ML_MSG_ID_COUNTER       = 0x05,  /* Counter Packet */
    SL_ML_MSG_ID_END_SESSION   = 0xFF   /* End Session Packet */
} sl_ml_profiler_msg_id_t;
#endif //(SL_ML_ENABLE_PROFILER_DEBUG_MSG)
#endif /* SL_ML_PROFILER_MSG_ID_H */