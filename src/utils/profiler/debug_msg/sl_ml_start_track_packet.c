/***************************************************************************//**
 * @file sl_ml_start_event_packets.c
 * @brief Build start track packet.
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
#include "sl_ml_start_track_packet.h"
#include "sl_ml_counter_unit_id.h"

/* Returns total packet length in bytes */
size_t build_start_track_packet(uint8_t *buf, size_t buf_len, sl_ml_profiler_start_track_info_t *start_track_info, uint8_t *track_uuids, uint8_t track_id_index) {
    if (buf_len < MIN_BUF_LENGTH_START_TRACK_PKT) {
        return 0; // Buffer too small
    }
    sl_ml_start_track_packet_t *pkt = (sl_ml_start_track_packet_t *)buf;
    size_t offset = 0;

    /* 1. Start with profiler_msg_id */
    pkt->profiler_msg_id = SL_ML_MSG_ID_START_TRACK;
    offset = sizeof(pkt->profiler_msg_id);
    /* 2. Copy track UUID */
    for (uint8_t i = 0; i < SL_ML_TRACK_UUID_BYTES; ++i) {
        pkt->track_uuid[i] = track_uuids[SL_ML_TRACK_UUID_BYTES * track_id_index + i];   
    }
    offset += SL_ML_TRACK_UUID_BYTES;

    /* 3. Add processor_id from counter or processor core */
    //pkt->processor_id = start_track_info->processor_id;
    //offset += sizeof(pkt->processor_id);

    /* Helper macro to add TLV safely */
    #define ADD_TLV(_type, _valptr, _len) do {                        \
        if (offset + 2 + (_len) > buf_len) break;                     \
        buf[offset++] = (_type);                                      \
        buf[offset++] = (uint8_t)(_len);                              \
        for (uint8_t i = 0; i < (_len); ++i)                          \
            buf[offset++] = ((const uint8_t *)(_valptr))[i];          \
    } while (0)
    ADD_TLV(SL_ML_TLV_PROCESSOR_ID, &start_track_info->processor_id, sizeof(start_track_info->processor_id));

    /* 3. Add TLV */
    if (start_track_info->is_counter && start_track_info->has_parent){
        ADD_TLV(SL_ML_TLV_COUNTER_UNIT, &start_track_info->counter_unit, sizeof(start_track_info->counter_unit));
    }
    
    /* 4. Add parent track UUID if present */
    if (start_track_info->has_parent){
        ADD_TLV(SL_ML_TLV_PARENT_TRACK_UUID, &track_uuids[0], SL_ML_TRACK_UUID_BYTES);
    }
    
    #undef ADD_TLV
    
    return offset; /* total payload size */
}
#endif //(SL_ML_ENABLE_PROFILER_DEBUG_MSG)
