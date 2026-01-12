/***************************************************************************//**
 * @file sl_ml_start_event_packets.h
 * @brief Header to create start event packet.
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
#ifndef SL_ML_START_EVENT_PACKET_H
#define SL_ML_START_EVENT_PACKET_H

#include "sl_ml_profiler_debug_channel.h"
#if (SL_ML_ENABLE_PROFILER_DEBUG_MSG)
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "sl_ml_profiler_msg_id.h"
#include "sl_tlv.h"

#define BUFFER_LENGTH_START_EVENT_PKT 128
#define MIN_BUFFER_LENGTH_START_EVENT_PKT 28

#pragma pack(push, 1)

/* TLV type identifiers for Start Event Packet */
#define SL_ML_TLV_LAYER_INPUT_SHAPE   0x30
#define SL_ML_TLV_LAYER_OUTPUT_SHAPE  0x31


/* Start Event Packet payload (from overall frame byte 19) */
typedef struct {
    sl_ml_profiler_msg_id_t profiler_msg_id; /* SL_ML_MSG_ID_START_EVENT (0x03) */
    uint8_t trusted_packet_seq_id;           /* trusted sequence ID */
    uint8_t track_uuid[8];                   /* byte 0 = LSB ... byte 7 = MSB */
    char event_function_name[16];            /* 16-byte function name identifier */
    uint16_t event_function_line_num;        /* source line number representing function */
    sl_tlv_t tlvs[];                         /* variable-length TLV fields */
} sl_ml_start_event_packet_t;

#pragma pack(pop)

#ifdef __cplusplus
extern "C" {
#endif
size_t build_start_event_packet(uint8_t *buf, size_t buf_len, sl_ml_profiler_event_info_t *start_event_info, uint8_t trusted_packet_seq_id, uint8_t *track_uuid);
#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
static_assert(sizeof(sl_ml_start_event_packet_t) == MIN_BUFFER_LENGTH_START_EVENT_PKT, "Unexpected packing for sl_ml_start_event_packet_t");
#endif

#endif //(SL_ML_ENABLE_PROFILER_DEBUG_MSG)
#endif /* SL_ML_START_EVENT_PACKET_H */
