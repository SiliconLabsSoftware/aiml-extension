/***************************************************************************//**
 * @file sl_ml_couner_packets.c
 * @brief Build performance counter packet.
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
#include "sl_ml_counter_packet.h"
#if (SL_ML_ENABLE_PROFILER_DEBUG_MSG)
/* Returns total packet length in bytes */
size_t build_counter_packet(uint8_t *buf, size_t buf_len, sl_ml_profiler_counter_pkt_info_t *counter_pkt_info, uint8_t *track_uuid, uint8_t trusted_packet_seq_id) {
    if (buf_len < MIN_BUFFER_LENGTH_COUNTER_PKT) {
        return 0; // Buffer too small
    }
    sl_ml_counter_packet_t *pkt = (sl_ml_counter_packet_t *)buf;
    size_t offset = 0;

    /* 1. Start with profiler_msg_id */
    pkt->profiler_msg_id = SL_ML_MSG_ID_COUNTER;
    offset = sizeof(pkt->profiler_msg_id);
    /* 2. Copy track UUID */
    for (uint8_t i = 0; i < 8; ++i) {
        pkt->track_uuid[i] = track_uuid[i];
    }
    offset += 8;
    /* 3. Add counter_value */
    pkt->counter_value = counter_pkt_info->counter_value;
    offset += sizeof(pkt->counter_value);
    /* 4. Add trusted_packet_seq_id */
    pkt->trusted_packet_seq_id = trusted_packet_seq_id;
    offset += sizeof(pkt->trusted_packet_seq_id);
    
    return offset; /* total payload size */
}
#endif
