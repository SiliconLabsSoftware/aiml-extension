/***************************************************************************//**
 * @file sl_ml_end_session_packet.h
 * @brief Define message signal to SDM to stop a capture session and gracefully release any resources.
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
#ifndef END_SESSION_PACKET_H
#define END_SESSION_PACKET_H

#include "sl_ml_profiler_debug_channel.h"
#if (SL_ML_ENABLE_PROFILER_DEBUG_MSG)

#include <stdint.h>
#include <stddef.h>
#include "sl_ml_profiler_msg_id.h"
#include "sl_tlv.h"

#define MIN_BUFFER_LENGTH_END_SESSION_PKT  1
#define BUFFER_LENGTH_END_SESSION_PKT 1024

#pragma pack(push, 1)

/* TLV type identifiers for End Session summary */
#define TLV_TOTAL_NUM_CYCLES         0x20  /* count (e.g., uint64, LE) */
#define TLV_TOTAL_NUM_STALLS         0x21  /* count (uint64, LE) */
#define TLV_TOTAL_ACC_CYCLES         0x22  /* count (uint64, LE) */
#define TLV_TOTAL_ACC_STALLS         0x23  /* count (uint64, LE) */
#define TLV_TOTAL_NUM_OPS            0x24  /* count (uint64, LE) */
#define TLV_TOTAL_NUM_MACS           0x25  /* count (uint64, LE) */
#define TLV_TOTAL_FLASH_USED_KB      0x26  /* size in KB (uint32, LE) */
#define TLV_TOTAL_RAM_USED_KB        0x27  /* size in KB (uint32, LE) */
#define TLV_MEAN_MAC_PER_CYCLE       0x28  /* float (IEEE-754, f32, LE) */
#define TLV_MEAN_INFERENCE_TIME_MS   0x29  /* float (IEEE-754, f32, LE) */
#if defined(SL_ML_ENABLE_ENERGY_PROFILING)
#define TLV_ENERGY_JOULES            0x2A  /* float (IEEE-754, f32, LE) */
#endif // SL_ML_ENABLE_ENERGY_PROFILING

/* End Session Packet payload (starts at overall frame byte 19) */
typedef struct {
    sl_ml_profiler_msg_id_t profiler_msg_id; /* must be SL_ML_MSG_ID_END_SESSION (0xFF) */
    sl_tlv_t tlvs[];                         /* sequence of sl_tlv_t entries packed back-to-back */
} sl_ml_end_session_packet_t;

#pragma pack(pop)

#ifdef __cplusplus
extern "C" {
#endif

size_t build_end_session_packet(uint8_t *buf, size_t buf_len, sl_ml_profiler_end_session_info_t *end_session_info);

#ifdef __cplusplus
} // extern "C"
#endif
#endif //(SL_ML_ENABLE_PROFILER_DEBUG_MSG)
#endif /* END_SESSION_PACKET_H */