
/***************************************************************************//**
 * @file
 * @brief Silicon Labs Arducam camera peripheral driver
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
#pragma once


#include <stdint.h>
#include "sl_status.h"



#define OV2640_ID 0x26
#define OV2640_I2C_ADDRESS 0x30


typedef struct
{
    uint8_t Manufacturer_ID1;
    uint8_t Manufacturer_ID2;
    uint8_t PIDH;
    uint8_t PIDL;
} ov2640_id_t;


sl_status_t slx_ml_ov2640_init(const arducam_config_t *config);
sl_status_t slx_ml_ov2640_deinit();
sl_status_t slx_ml_ov2640_set_setting(arducam_setting_t setting, int32_t value);
sl_status_t slx_ml_ov2640_get_setting(arducam_setting_t setting, int32_t *value_ptr);

//sl_status_t ov2640_set_zoom_and_pan(int zoom, int hpan, int vpan, int x_dither, int y_dither);
