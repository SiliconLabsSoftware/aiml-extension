/***************************************************************************//**
 * @file sl_ml_start_session_packet.h
 * @brief Builds debug message signal to SDM to start a capture session and allocate any resources required.
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
#include "sl_ml_start_session_packet.h"
#if (SL_ML_ENABLE_PROFILER_DEBUG_MSG)
#include "sl_ml_mcu_id.h"
#include "sl_ml_accelerator_id.h"
#include "string.h"

/* Returns total packet length in bytes */
size_t build_start_session_packet(uint8_t *buf, size_t buf_len, sl_ml_profiler_start_session_info_t *start_session_info) {
    sl_ml_start_session_packet_t *pkt = (sl_ml_start_session_packet_t *)buf;
    size_t offset = 0;

    /* 1. Start with profiler_msg_id */
    pkt->profiler_msg_id = SL_ML_MSG_ID_START_SESSION;
    offset = sizeof(pkt->profiler_msg_id);

    /* Helper macro to add TLV safely */
    #define ADD_TLV(_type, _valptr, _len) do {                        \
        if (offset + 2 + (_len) > buf_len) break;                     \
        buf[offset++] = (_type);                                      \
        buf[offset++] = (uint8_t)(_len);                              \
        for (uint8_t i = 0; i < (_len); ++i)                          \
            buf[offset++] = ((const uint8_t *)(_valptr))[i];          \
    } while (0)

    /* 2. Add TLVs */
    /*const char model_name[] = "keyword_spotting_on_off_v3";
    const char board_name[] = "BRD2601b";
    const char opn[] = "EFR32MG24B310F1536IM48";
    const char part_family[] = "xG24";
    uint16_t flash_kb = 1536;
    uint16_t ram_kb = 256;
    uint16_t arena_kb = 128;
    uint8_t mcu_id = SL_ML_MCU_ID_ARM_CORTEX_M33;
    uint8_t accel_id = SL_ML_ACCELERATOR_ID_MVPv1;*/

    ADD_TLV(TLV_MODEL_NAME, start_session_info->model_name, strlen(start_session_info->model_name)); // -1 removed strlen does not count the null terminator
    ADD_TLV(TLV_BOARD_NAME, start_session_info->board_name, strlen(start_session_info->board_name));
    ADD_TLV(TLV_OPN, start_session_info->opn, strlen(start_session_info->opn));
    ADD_TLV(TLV_PART_FAMILY, start_session_info->part_family, strlen(start_session_info->part_family));
    ADD_TLV(TLV_FLASH_SIZE_KB, &start_session_info->flash_kb, sizeof(start_session_info->flash_kb));
    ADD_TLV(TLV_RAM_SIZE_KB, &start_session_info->ram_kb, sizeof(start_session_info->ram_kb));
    ADD_TLV(TLV_ARENA_SIZE_KB, &start_session_info->arena_kb, sizeof(start_session_info->arena_kb));
    ADD_TLV(TLV_MCU_ID, &start_session_info->mcu_id, sizeof(uint8_t));
    ADD_TLV(TLV_ACCELERATOR_ID, &start_session_info->accel_id, sizeof(uint8_t));

    #undef ADD_TLV
    if (offset > buf_len)
    {
        return 0;
    }
    
    return offset; /* total payload size */
}
#endif //(SL_ML_ENABLE_PROFILER_DEBUG_MSG)
