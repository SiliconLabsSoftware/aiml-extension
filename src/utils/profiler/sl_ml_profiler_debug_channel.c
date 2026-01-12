/***************************************************************************//**
 * @file sl_ml_profiler_debug_channel.c
 * @brief Writes debug message to swo channel.
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
#include "sl_ml_start_session_packet.h"
#include "sl_ml_end_session_packet.h"
#include "sl_ml_start_event_packet.h"
#include "sl_ml_end_event_packet.h"
#include "sl_ml_start_track_packet.h"
#include "sl_ml_end_track_packet.h"
#include "sl_ml_counter_packet.h"
#include "uuid_embedded.h"
#include <stdlib.h>

#if defined(SL_BOARD_SI91X)
#include "sl_si91x_iostream_debug.h"
#endif // SL_BOARD_SI91X

#if defined(SL_BOARD_EFX)
#include "sl_iostream_debug.h"
#endif // SL_BOARD_EFX

// Track UUID used for start/stop track packets
// +1 for top track id, can be merged later in to track packet buffers to save memory
static uint8_t counter_track_ids[SL_ML_TRACK_UUID_BYTES * (SL_ML_COUNTER_UNIT_ID_LEN + 1)];
static uint8_t core_track_ids[SL_ML_TRACK_UUID_BYTES * (SL_ML_PROCESSOR_ID_LEN + 1)];
static uint8_t trusted_packet_seq_id = 0x90;

static void sl_ml_uuid_init()
{
  uint64_t seed = ((uint64_t)platform_entropy32() << 32) ^ platform_entropy32() ^ 0xA5A5A5A5A5A5A5A5ULL;
  uuid_seed(seed);

  // generate track UUIDs
  // Initiate uuids
  memset(counter_track_ids, 0, sizeof(counter_track_ids));
  memset(core_track_ids, 0, sizeof(core_track_ids));
  for (int i = 0; i < SL_ML_COUNTER_UNIT_ID_LEN+1; i++) {
      uuid_randbytes(&counter_track_ids[i * SL_ML_TRACK_UUID_BYTES], SL_ML_TRACK_UUID_BYTES);
  }
  for (int i = 0; i < SL_ML_PROCESSOR_ID_LEN+1; i++) {
      uuid_randbytes(&core_track_ids[i * SL_ML_TRACK_UUID_BYTES], SL_ML_TRACK_UUID_BYTES);
  }
}
#if defined(SL_BOARD_SI91X)
/* ***************************************************************************//**
*  @brief Initialize debug channel for profiler
*******************************************************************************/
void sl_ml_profiler_debug_init()
{
  // Configure SWO stimulus 8
  //context = sli_iostream_swo_itm_8_init();
  sl_si91x_iostream_debug_init();
  sl_si91x_iostream_set_debug_type(SI91X_DEBUG_ML_PROFILER);
  //debug_stream = sl_iostream_get_default();
  sl_ml_uuid_init();
}
/* ***************************************************************************//**
*  @brief Helper function to write packet to debug channel
*******************************************************************************/
static void sl_ml_profiler_debug_write(uint8_t *buffer, uint32_t byteWritten)
{
    //sli_iostream_swo_itm_8_write(context, buffer,byteWritten,EM_DEBUG_ML_PROFILER, sequence_number);// EM_DEBUG_ML_PROFILER
    //sequence_number ++;
    sl_status_t status = SL_STATUS_OK;
    status             = sl_iostream_write(sl_si91x_iostream_debug_handle, buffer, byteWritten);
    if (status != SL_STATUS_OK) {
      exit(1);
    }
}
#endif// SL_BOARD_91X

#if defined(SL_BOARD_EFX)
/* ***************************************************************************//**
*  @brief Initialize debug channel for profiler
*******************************************************************************/
void sl_ml_profiler_debug_init()
{
  sl_iostream_debug_init();
  sl_iostream_set_debug_type(EM_DEBUG_ML_PROFILER);
  sl_ml_uuid_init();
}
/* ***************************************************************************//**
*  @brief Helper function to write packet to debug channel
*******************************************************************************/
static void sl_ml_profiler_debug_write(uint8_t *buffer, uint32_t byteWritten)
{
    sl_status_t status = SL_STATUS_OK;
    status             = sl_iostream_write(sl_iostream_debug_handle, buffer, byteWritten);
    if (status != SL_STATUS_OK) {
      exit(1);
    }
}
#endif // SL_BOARD_EFX
/* ***************************************************************************//**
*  @brief Start session packet write to debug channel
*******************************************************************************/
void sl_ml_profiler_start_session_write(sl_ml_profiler_start_session_info_t *start_session_info)
{
  uint8_t buffer[BUFFER_LENGTH_START_SESSION_PKT];
  int byteWritten = build_start_session_packet(buffer, BUFFER_LENGTH_START_SESSION_PKT, start_session_info);
  sl_ml_profiler_debug_write(buffer, byteWritten);
}
/* ***************************************************************************//**
*  @brief End session packet write to debug channel
*******************************************************************************/
void sl_ml_profiler_end_session_write(sl_ml_profiler_end_session_info_t *end_session_info)
{
  uint8_t buffer[BUFFER_LENGTH_END_SESSION_PKT];
  int byteWritten = build_end_session_packet(buffer, BUFFER_LENGTH_END_SESSION_PKT, end_session_info);
  sl_ml_profiler_debug_write(buffer, byteWritten);
}
/* ***************************************************************************//**
*  @brief Start event packet write to debug channel
*******************************************************************************/
void sl_ml_profiler_start_event_write(sl_ml_profiler_event_info_t *start_event_info)
{
    uint8_t buffer[BUFFER_LENGTH_START_EVENT_PKT];

    int16_t track_id_index = get_processor_id_index(start_event_info->processor_id);

    if (track_id_index < 0) {// index should not be negative
      exit(1);
    }
    int byteWritten = build_start_event_packet(buffer, BUFFER_LENGTH_START_EVENT_PKT, start_event_info, trusted_packet_seq_id, 
      &core_track_ids[(track_id_index+1)*SL_ML_TRACK_UUID_BYTES]);
      
    sl_ml_profiler_debug_write(buffer, byteWritten);
}
/* ***************************************************************************//**
*  @brief End event packet write to debug channel
*******************************************************************************/
void sl_ml_profiler_end_event_write(sl_ml_profiler_event_info_t *end_event_info)
{
    uint8_t buffer[BUFFER_LENGTH_END_EVENT_PKT];
    int16_t track_id_index = get_processor_id_index(end_event_info->processor_id);
    if (track_id_index < 0) {// index should not be negative
      exit(1);
    }
    int byteWritten = build_end_event_packet(buffer, BUFFER_LENGTH_END_EVENT_PKT, end_event_info, trusted_packet_seq_id, 
      &core_track_ids[(track_id_index+1)*SL_ML_TRACK_UUID_BYTES]);

    sl_ml_profiler_debug_write(buffer, byteWritten);
}
/* ***************************************************************************//**
*  @brief Start processor core track packet write to debug channel
*******************************************************************************/
void sl_ml_profiler_counter_start_track_write(sl_ml_profiler_start_track_info_t *start_track_info)
{
  uint8_t buffer[START_TRACK_PACKET_BUF_SIZE];
  int16_t track_id_index=0;
  uint16_t byteWritten = 0;
  if (start_track_info->has_parent){
      track_id_index = get_counter_id_index(start_track_info->processor_id);
      if (track_id_index < 0) {// index should not be negative
        exit(1);
      }
      track_id_index += 1; // +1 for top track id
  }
  byteWritten = build_start_track_packet(buffer, START_TRACK_PACKET_BUF_SIZE, start_track_info, counter_track_ids, track_id_index);
  sl_ml_profiler_debug_write(buffer, byteWritten);
}

/* ***************************************************************************//**
*  @brief Start processor core track packet write to debug channel
*******************************************************************************/
void sl_ml_profiler_core_start_track_write(sl_ml_profiler_start_track_info_t *start_track_info)
{
  uint8_t buffer[START_TRACK_PACKET_BUF_SIZE];
  int16_t track_id_index=0;
  uint16_t byteWritten = 0;
  if (start_track_info->has_parent){
      track_id_index = get_processor_id_index(start_track_info->processor_id);
      if (track_id_index < 0) {
        exit(1); // index should not be negative
      }
      track_id_index += 1; // +1 for top track id
  }
  byteWritten = build_start_track_packet(buffer, START_TRACK_PACKET_BUF_SIZE, start_track_info, core_track_ids, track_id_index);
  sl_ml_profiler_debug_write(buffer, byteWritten);
}
/* ***************************************************************************//**
*  @brief End processor core track packet write to debug channel
*******************************************************************************/
void sl_ml_profiler_counter_end_track_write(sl_ml_profiler_end_track_info_t *end_track_info)
{
  uint8_t buffer[END_TRACK_PACKET_BUF_SIZE];
  int16_t track_id_index=0;
  uint16_t byteWritten = 0;
  if (end_track_info->has_parent){
      track_id_index = get_counter_id_index(end_track_info->processor_id);
      if (track_id_index < 0) {// index should not be negative
        exit(1);
      }
      track_id_index += 1; // +1 for top track id
  }
  byteWritten = build_end_track_packet(buffer, END_TRACK_PACKET_BUF_SIZE, end_track_info, counter_track_ids, track_id_index);
  sl_ml_profiler_debug_write(buffer, byteWritten);
}

/* ***************************************************************************//**
*  @brief End processor core track packet write to debug channel
*******************************************************************************/
void sl_ml_profiler_core_end_track_write(sl_ml_profiler_end_track_info_t *end_track_info)
{
  uint8_t buffer[END_TRACK_PACKET_BUF_SIZE];
  int16_t track_id_index=0;
  uint16_t byteWritten = 0;
  if (end_track_info->has_parent){
      track_id_index = get_processor_id_index(end_track_info->processor_id);
      if (track_id_index < 0) {
        exit(1); // index should not be negative
      }
      track_id_index += 1; // +1 for top track id
  }
  byteWritten = build_end_track_packet(buffer, END_TRACK_PACKET_BUF_SIZE, end_track_info, core_track_ids, track_id_index);
  sl_ml_profiler_debug_write(buffer, byteWritten);
}
/* ***************************************************************************//**
*  @brief End track packet write to debug channel
*******************************************************************************/
void sl_ml_profiler_counter_pkt_write(sl_ml_profiler_counter_pkt_info_t *counter_info)
{
    uint8_t buffer[COUNTER_PACKET_BUF_SIZE];
    int16_t track_id_index = get_counter_unit_index(counter_info->counter_unit);
    if (track_id_index < 0) {// index should not be negative
      exit(1);
    }
    track_id_index += 1; // +1 for top track id
    int byteWritten = build_counter_packet(buffer, COUNTER_PACKET_BUF_SIZE, counter_info, &counter_track_ids[SL_ML_TRACK_UUID_BYTES * (track_id_index)], trusted_packet_seq_id);
    sl_ml_profiler_debug_write(buffer, byteWritten);
}

#endif // SL_ML_ENABLE_PROFILER_DEBUG_MSG
