/***************************************************************************//**
 * @file sl_ml_start_session_packet.h
 * @brief Define message signal to SDM to start a capture session and allocate any resources required.
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

#ifndef START_SESSION_PACKET_H
#define START_SESSION_PACKET_H

#include "sl_ml_profiler_debug_channel.h"
#if (SL_ML_ENABLE_PROFILER_DEBUG_MSG)

#include <stddef.h>
#include <stdint.h>
#include "sl_ml_profiler_msg_id.h"
#include "sl_tlv.h"

#define BUFFER_LENGTH_START_SESSION_PKT 256

#pragma pack(push, 1)

/* TLV type identifiers (assign/extend as needed) */
#define TLV_MODEL_NAME            0x01  /* ASCII */ //Approx 32 bytes
#define TLV_BOARD_NAME            0x10  /* ASCII, e.g., "BRD2601b" */
#define TLV_OPN                   0x11  /* ASCII, e.g., "EFR32MG24B310F1536IM48" */
#define TLV_PART_FAMILY           0x12  /* ASCII, e.g., "xG24" */
#define TLV_FLASH_SIZE_KB         0x13  /* uint16, little-endian */
#define TLV_RAM_SIZE_KB           0x14  /* uint16, little-endian */
#define TLV_ARENA_SIZE_KB         0x15  /* uint16, little-endian */
#define TLV_MCU_ID                0x16  /* uint8, enum representing MCU name, e.g., 0x00 for ARM Cortex-M33 */
#define TLV_ACCELERATOR_ID        0x17  /* uint8, enum representing accelerator name, e.g., 0x00 for MVPv1 */

/* Start Session Packet payload (begins at overall frame byte 19) */
typedef struct {
  sl_ml_profiler_msg_id_t profiler_msg_id; /* must be SL_ML_MSG_ID_START_SESSION (0x00) */
  sl_tlv_t tlvs[];                         /* sequence of ss_tlv_t entries packed back-to-back */
} sl_ml_start_session_packet_t;

#pragma pack(pop)

/* --- Optional little-endian helpers for 16-bit TLV values --- */
static inline uint16_t ss_le16_read(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline void ss_le16_write(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

#ifdef __cplusplus
extern "C" {
#endif
size_t build_start_session_packet(uint8_t *buf, size_t buf_len, sl_ml_profiler_start_session_info_t *start_session_info);
#ifdef __cplusplus
} // extern "C"
#endif

#endif //(SL_ML_ENABLE_PROFILER_DEBUG_MSG)
#endif /* START_SESSION_PACKET_H */
