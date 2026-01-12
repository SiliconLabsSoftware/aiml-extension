/***************************************************************************//**
 * @file sl_ml_end_session_packet.c
 * @brief Builds debug message to SDM to stop a capture session and gracefully release any resources.
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
#include "sl_ml_end_session_packet.h"
#if (SL_ML_ENABLE_PROFILER_DEBUG_MSG)
/* Returns total packet length in bytes */
size_t build_end_session_packet(uint8_t *buf, size_t buf_len, sl_ml_profiler_end_session_info_t *end_session_info) {
    sl_ml_end_session_packet_t *pkt = (sl_ml_end_session_packet_t *)buf;
    size_t offset = 0;

    /* 1. Start with profiler_msg_id */
    pkt->profiler_msg_id = SL_ML_MSG_ID_END_SESSION;
    offset = sizeof(pkt->profiler_msg_id);

    /* Helper macro to add TLV safely */
    #define ADD_TLV(_type, _valptr, _len) do {                        \
        if (offset + 2 + (_len) > buf_len) break;                     \
        buf[offset++] = (_type);                                      \
        buf[offset++] = (uint8_t)(_len);                              \
        for (uint8_t i = 0; i < (_len); ++i)                          \
            buf[offset++] = ((const uint8_t *)(_valptr))[i];          \
    } while (0)

    ADD_TLV(TLV_TOTAL_NUM_CYCLES, &end_session_info->total_num_cycles, sizeof(end_session_info->total_num_cycles));
    ADD_TLV(TLV_TOTAL_NUM_STALLS, &end_session_info->total_num_stalls, sizeof(end_session_info->total_num_stalls));
    ADD_TLV(TLV_TOTAL_ACC_CYCLES, &end_session_info->total_acc_cycles, sizeof(end_session_info->total_acc_cycles));
    ADD_TLV(TLV_TOTAL_ACC_STALLS, &end_session_info->total_acc_stalls, sizeof(end_session_info->total_acc_stalls));
    ADD_TLV(TLV_TOTAL_NUM_OPS, &end_session_info->total_num_ops, sizeof(end_session_info->total_num_ops));
    ADD_TLV(TLV_TOTAL_NUM_MACS, &end_session_info->total_num_macs, sizeof(end_session_info->total_num_macs));
    ADD_TLV(TLV_TOTAL_FLASH_USED_KB, &end_session_info->total_flash_used_kb, sizeof(end_session_info->total_flash_used_kb));
    ADD_TLV(TLV_TOTAL_RAM_USED_KB, &end_session_info->total_ram_used_kb, sizeof(end_session_info->total_ram_used_kb));
    ADD_TLV(TLV_MEAN_MAC_PER_CYCLE, &end_session_info->mean_mac_per_cycle, sizeof(end_session_info->mean_mac_per_cycle));
    ADD_TLV(TLV_MEAN_INFERENCE_TIME_MS, &end_session_info->mean_inference_time_ms, sizeof(end_session_info->mean_inference_time_ms));
#ifdef SL_ML_ENABLE_ENERGY_PROFILING
    ADD_TLV(TLV_ENERGY_JOULES, &end_session_info->energy_joules, sizeof(end_session_info->energy_joules));
#endif // SL_ML_ENABLE_ENERGY_PROFILING
    #undef ADD_TLV
    if (offset > buf_len)
    {
        return 0;
    }

    return offset; /* total payload size */
}
#endif //(SL_ML_ENABLE_PROFILER_DEBUG_MSG)
