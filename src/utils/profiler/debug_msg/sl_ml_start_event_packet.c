/***************************************************************************//**
 * @file sl_ml_start_event_packets.c
 * @brief Build start event packet.
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
#include "sl_ml_start_event_packet.h"
#if (SL_ML_ENABLE_PROFILER_DEBUG_MSG)
/* Returns total packet length in bytes */
size_t build_start_event_packet(uint8_t *buf, size_t buf_len, sl_ml_profiler_event_info_t *start_event_info, uint8_t trusted_packet_seq_id, uint8_t *track_uuid) {
    
    if (buf_len < MIN_BUFFER_LENGTH_START_EVENT_PKT) {
        return 0; // Buffer too small
    }
    sl_ml_start_event_packet_t *pkt = (sl_ml_start_event_packet_t *)buf;
    size_t offset = 0;

    /* 1. Start with profiler_msg_id */
    pkt->profiler_msg_id = SL_ML_MSG_ID_START_EVENT;
    offset = sizeof(pkt->profiler_msg_id);
    pkt->trusted_packet_seq_id = trusted_packet_seq_id;
    offset += sizeof(pkt->trusted_packet_seq_id);
    /* 2. Copy track UUID */
    for (uint8_t i = 0; i < 8; ++i) {
        pkt->track_uuid[i] = track_uuid[i];   
    }
    offset += 8;
    /* 3. Copy function name (16 bytes) */
    size_t fn_name_len = strlen(start_event_info->function_name);
    memset(pkt->event_function_name, 0, 16); 
    memcpy(pkt->event_function_name, start_event_info->function_name, MIN(fn_name_len, 16));
    offset += 16;
    /* 4. Copy function line number */
    pkt->event_function_line_num = start_event_info->line_number;  
    offset += sizeof(pkt->event_function_line_num);

    /* Helper macro to add TLV safely */
    #define ADD_TLV(_type, _valptr, _len) do {                        \
        if (offset + 2 + (_len) > buf_len) break;                     \
        buf[offset++] = (_type);                                      \
        buf[offset++] = (uint8_t)(_len);                              \
        for (uint8_t i = 0; i < (_len); ++i)                          \
            buf[offset++] = ((const uint8_t *)(_valptr))[i];          \
    } while (0)

    /* 5. Add TLVs for input/output shapes */
    if (start_event_info->input_shape.n_dimensions > 4 || start_event_info->output_shape.n_dimensions > 4) {
        return 0;
    } else {
        ADD_TLV(SL_ML_TLV_LAYER_INPUT_SHAPE, start_event_info->input_shape.shape, sizeof(uint16_t) * start_event_info->input_shape.n_dimensions);
        ADD_TLV(SL_ML_TLV_LAYER_OUTPUT_SHAPE, start_event_info->output_shape.shape, sizeof(uint16_t) * start_event_info->output_shape.n_dimensions);
    }

    #undef ADD_TLV

    return offset; /* total payload size */
}
#endif //(SL_ML_ENABLE_PROFILER_DEBUG_MSG)
