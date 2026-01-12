/***************************************************************************//**
 * @file sl_ml_counter_packet.h
 * @brief Header to create counter packet.
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
#ifndef SL_ML_COUNTER_PACKET_H
#define SL_ML_COUNTER_PACKET_H

#include "sl_ml_profiler_debug_channel.h"
#if (SL_ML_ENABLE_PROFILER_DEBUG_MSG)
#include <stdint.h>
#include "sl_ml_profiler_msg_id.h"

#define MIN_BUFFER_LENGTH_COUNTER_PKT 18
#pragma pack(push, 1)

/* Counter Packet payload (from overall frame byte 19) */
typedef struct {
    sl_ml_profiler_msg_id_t profiler_msg_id;   /* SL_ML_MSG_ID_COUNTER (0x05) */
    uint8_t  track_uuid[8];                    /* byte 0 = LSB ... byte 7 = MSB */
    double   counter_value;                    /* instantaneous Perfetto counter value */
    uint8_t  trusted_packet_seq_id;            /* sequence ID for integrity/ordering */
} sl_ml_counter_packet_t;

#pragma pack(pop)
#ifdef __cplusplus
extern "C" {
#endif
size_t build_counter_packet(uint8_t *buf, size_t buf_len, sl_ml_profiler_counter_pkt_info_t *counter_pkt_info, uint8_t *track_uuid, uint8_t trusted_packet_seq_id);
#ifdef __cplusplus
}
#endif
#endif // (SL_ML_ENABLE_PROFILER_DEBUG_MSG)
#endif /* SL_ML_COUNTER_PACKET_H */
