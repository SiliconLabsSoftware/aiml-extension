/***************************************************************************//**
 * @file sl_ml_end_event_packets.h
 * @brief Header to create end event packet.
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
#ifndef SL_ML_END_EVENT_PACKET_H
#define SL_ML_END_EVENT_PACKET_H

#include "sl_ml_profiler_debug_channel.h"
#if (SL_ML_ENABLE_PROFILER_DEBUG_MSG)

#include <stddef.h>
#include <stdint.h>
#include "sl_ml_profiler_msg_id.h"
#include "sl_tlv.h"

#define MIN_BUFFER_LENGTH_END_EVENT_PKT 28
#define BUFFER_LENGTH_END_EVENT_PKT 512

#pragma pack(push, 1)

/* TLV type identifiers for End Event Packet */
#define SL_ML_TLV_NUM_CPU_CYCLES     0x40  // 64-bit unsigned integer
#define SL_ML_TLV_NUM_CPU_STALLS     0x41  // 64-bit unsigned integer
#define SL_ML_TLV_NUM_MVP_CYCLES     0x42  // 64-bit unsigned integer
#define SL_ML_TLV_NUM_MVP_STALLS     0x43  // 64-bit unsigned integer
#define SL_ML_TLV_CPU_UTIL_PERCENT   0x44  // 32-bit float
#define SL_ML_TLV_CLOCK_RATE_HZ      0x45  // 32-bit float
#define SL_ML_TLV_MAC_PER_CYCLE      0x46  // 32-bit float
#if defined(SL_ML_ENABLE_ENERGY_PROFILING)
#define SL_ML_TLV_ENERGY_JOULES      0x47  // 32-bit float
#endif

/* End Event Packet payload (from overall frame byte 19) */
typedef struct {
    sl_ml_profiler_msg_id_t profiler_msg_id; /* SL_ML_MSG_ID_END_EVENT (0x04) */
    uint8_t trusted_packet_seq_id;           /* trusted sequence ID */
    uint8_t track_uuid[8];                   /* byte 0 = LSB ... byte 7 = MSB */
    uint8_t event_function_name[16];         /* 16-byte function name identifier */
    uint16_t event_function_line_num;        /* line number representing function */
    sl_tlv_t tlvs[];                         /* variable-length TLV fields */
} sl_ml_end_event_packet_t;

#pragma pack(pop)
#ifdef __cplusplus
extern "C" {
#endif

size_t build_end_event_packet(uint8_t *buf, size_t buf_len, sl_ml_profiler_event_info_t *end_event_info, uint8_t trusted_packet_seq_id, uint8_t *track_uuid);

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
static_assert(sizeof(sl_ml_end_event_packet_t) == MIN_BUFFER_LENGTH_END_EVENT_PKT, "Unexpected packing for sl_ml_end_event_packet_t");
#endif
#endif //(SL_ML_ENABLE_PROFILER_DEBUG_MSG)
#endif /* SL_ML_END_EVENT_PACKET_H */
