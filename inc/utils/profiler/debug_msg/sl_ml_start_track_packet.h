/***************************************************************************//**
 * @file sl_ml_start_track_packets.h
 * @brief Header to create start track packet.
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
#ifndef SL_ML_START_TRACK_PACKET_H
#define SL_ML_START_TRACK_PACKET_H

#include "sl_ml_profiler_debug_channel.h"
#if (SL_ML_ENABLE_PROFILER_DEBUG_MSG)

#include <stddef.h>
#include <stdint.h>
#include "sl_ml_profiler_msg_id.h"
#include "sl_ml_processor_id.h"
#include "sl_tlv.h"

#define MIN_BUF_LENGTH_START_TRACK_PKT 9
#define BUF_LEN_START_TRACK_PKT 22

#pragma pack(push, 1)

#define SL_ML_TLV_PROCESSOR_ID  0x50  /* uint8, enum representing processor core */
#define SL_ML_TLV_COUNTER_UNIT  0x51  /* uint8, enum representing counter unit */
#define SL_ML_TLV_PARENT_TRACK_UUID  0x52  /* ASCII, counter description string */

/* Start Track Packet payload (from overall frame byte 19) */
typedef struct {
    sl_ml_profiler_msg_id_t profiler_msg_id; /* SL_ML_MSG_ID_START_TRACK (0x01) */
    uint8_t track_uuid[8];                   /* byte 0 = LSB ... byte 7 = MSB */
    //sl_ml_processor_id_t processor_id;       /* MCU/accelerator identifier */
    //uint8_t is_counter;                      /* 0 = event-type track, 1 = counter-type track */
    sl_tlv_t tlvs[];                         /* variable-length TLV fields */
} sl_ml_start_track_packet_t;

#pragma pack(pop)

#ifdef __cplusplus
extern "C" {
#endif
size_t build_start_track_packet(uint8_t *buf, size_t buf_len, sl_ml_profiler_start_track_info_t *start_track_info, uint8_t *track_uuids, uint8_t track_id_index);
#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
static_assert(sizeof(sl_ml_start_track_packet_t) == MIN_BUF_LENGTH_START_TRACK_PKT, "Unexpected packing for sl_ml_start_track_packet_t");
#endif

#endif //(SL_ML_ENABLE_PROFILER_DEBUG_MSG)
#endif /* SL_ML_START_TRACK_PACKET_H */
