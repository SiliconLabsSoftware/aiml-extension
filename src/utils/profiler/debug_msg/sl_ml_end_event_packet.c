/***************************************************************************//**
 * @file sl_ml_end_event_packets.c
 * @brief Build end event packet.
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
#include "sl_ml_end_event_packet.h"
#if (SL_ML_ENABLE_PROFILER_DEBUG_MSG)
#include "string.h"

/* Returns total packet length in bytes */
size_t build_end_event_packet(uint8_t *buf, size_t buf_len, sl_ml_profiler_event_info_t *end_event_info, uint8_t trusted_packet_seq_id, uint8_t *track_uuid) {

    if (buf_len < MIN_BUFFER_LENGTH_END_EVENT_PKT) {
        return 0; // Buffer too small
    }
    sl_ml_end_event_packet_t *pkt = (sl_ml_end_event_packet_t *)buf;
    size_t offset = 0;

    /* 1. Start with profiler_msg_id */
    pkt->profiler_msg_id = SL_ML_MSG_ID_END_EVENT;
    offset = sizeof(pkt->profiler_msg_id);
    pkt->trusted_packet_seq_id = trusted_packet_seq_id;
    offset += sizeof(pkt->trusted_packet_seq_id);
    /* 2. Copy track UUID */
    for (uint8_t i = 0; i < 8; ++i) {
        pkt->track_uuid[i] = track_uuid[i];
    }
    offset += 8;
    /* 3. Copy function name (16 bytes) */
    size_t fn_name_len = strlen(end_event_info->function_name);
    memset(pkt->event_function_name, 0, 16);
    memcpy(pkt->event_function_name, end_event_info->function_name, MIN(fn_name_len,16));
    offset += 16;
    /* 4. Copy function line number */
    pkt->event_function_line_num = end_event_info->line_number;
    offset += sizeof(pkt->event_function_line_num);

    /* Helper macro to add TLV safely */
    #define ADD_TLV(_type, _valptr, _len) do {                        \
        if (offset + 2 + (_len) > buf_len) break;                     \
        buf[offset++] = (_type);                                      \
        buf[offset++] = (uint8_t)(_len);                              \
        for (uint8_t i = 0; i < (_len); ++i)                          \
            buf[offset++] = ((const uint8_t *)(_valptr))[i];          \
    } while (0)

    ADD_TLV(SL_ML_TLV_NUM_CPU_CYCLES, &end_event_info->num_cpu_cycles, sizeof(end_event_info->num_cpu_cycles));
    ADD_TLV(SL_ML_TLV_NUM_CPU_STALLS, &end_event_info->num_cpu_stalls, sizeof(end_event_info->num_cpu_stalls));
    ADD_TLV(SL_ML_TLV_NUM_MVP_CYCLES, &end_event_info->num_mvp_cycles, sizeof(end_event_info->num_mvp_cycles));
    ADD_TLV(SL_ML_TLV_NUM_MVP_STALLS, &end_event_info->num_mvp_stalls, sizeof(end_event_info->num_mvp_stalls));
    ADD_TLV(SL_ML_TLV_CPU_UTIL_PERCENT, &end_event_info->cpu_util_percent, sizeof(end_event_info->cpu_util_percent));
    ADD_TLV(SL_ML_TLV_CLOCK_RATE_HZ, &end_event_info->clock_rate_hz, sizeof(end_event_info->clock_rate_hz));
    ADD_TLV(SL_ML_TLV_MAC_PER_CYCLE, &end_event_info->mac_per_cycle, sizeof(end_event_info->mac_per_cycle));
#ifdef SL_ML_ENABLE_ENERGY_PROFILING
    ADD_TLV(SL_ML_TLV_ENERGY_JOULES, &end_event_info->energy_joules, sizeof(end_event_info->energy_joules));
#endif // SL_ML_ENABLE_ENERGY_PROFILING
    #undef ADD_TLV

    return offset; /* total payload size */
}
#endif //(SL_ML_ENABLE_PROFILER_DEBUG_MSG)
