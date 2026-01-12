/***************************************************************************//**
 * @file sl_ml_end_track_packets.c
 * @brief Build end track packet.
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

#include "sl_ml_profiler_debug_channel.h"
#if (SL_ML_ENABLE_PROFILER_DEBUG_MSG)
#include "sl_ml_end_track_packet.h"
/* Returns total packet length in bytes */
size_t build_end_track_packet(uint8_t *buf, size_t buf_len, sl_ml_profiler_end_track_info_t *end_track_info, uint8_t *track_uuids, uint8_t track_id_index) {
    if (buf_len < BUF_LEN_END_TRACK_PKT) {
        return 0; // Buffer too small
    }
    sl_ml_end_track_packet_t *pkt = (sl_ml_end_track_packet_t *)buf;
    size_t offset = 0;

    /* 1. Start with profiler_msg_id */
    pkt->profiler_msg_id = SL_ML_MSG_ID_END_TRACK;
    offset = sizeof(pkt->profiler_msg_id);
    /* 2. Copy track UUID */
    for (uint8_t i = 0; i < SL_ML_TRACK_UUID_BYTES; ++i) {
        pkt->track_uuid[i] = track_uuids[track_id_index * SL_ML_TRACK_UUID_BYTES + i];   
    }
    offset += SL_ML_TRACK_UUID_BYTES;
    /* 3. Add processor_id from counter or processor core */
    /*if (end_track_info->is_counter)
    {
        pkt->processor_id = end_track_info->counter_unit; // set to 0 for counter end track packet
    }
    else
    {*/
    pkt->processor_id = end_track_info->processor_id;
    //}
    offset += sizeof(pkt->processor_id);

    return offset; /* total payload size */
}
#endif
