
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

/***************************************************************************//**
 * @brief
 *  Driver function to initialize ov2640 camera driver.
 * 
 * @param[in] config 
 *  @ref arducam_config_t driver configuration
 * 
 * @return
 *  @ref sl_status_t SL_STATUS_OK for successful completion.
 ******************************************************************************/
sl_status_t slx_ml_ov2640_init(const arducam_config_t *config);

/***************************************************************************//**
 * @brief
 *  Driver function to set camera settings.
 * 
 * @param[in] setting 
 *  @ref arducam_setting_t camera setting to process.
 * 
 * @param[in] value 
 *  Value to set camera setting.
 * 
 * @return
 *  @ref sl_status_t SL_STATUS_OK for successful completion.
 ******************************************************************************/
sl_status_t slx_ml_ov2640_set_setting(arducam_setting_t setting, int32_t value);

 /***************************************************************************//**
 *  Local Functions
 ******************************************************************************/

/***************************************************************************//**
 * @brief
 *  Driver function to check camera compatibility.
 * 
 * @return
 *  @ref sl_status_t SL_STATUS_OK for supported camera,
 *       SL_STATUS_NOT_SUPPORTED if not supported.
 ******************************************************************************/
sl_status_t sli_check_comms_interface(void);

/***************************************************************************//**
 * @brief
 *  Internal driver function to set camera contrast.
 * 
 * @param[in] level 
 *  Integer value to set camera contrast[-2 to 2].
 * 
 * @return
 *  @ref sl_status_t SL_STATUS_OK for successful completion.
 ******************************************************************************/
sl_status_t sli_set_contrast(int level);

/***************************************************************************//**
 * @brief
 *  Internal driver function to set camera saturation.
 * 
 * @param[in] level 
 *  Integer value to set camera saturation[-2 to 2].
 * 
 * @return
 *  @ref sl_status_t SL_STATUS_OK for successful completion.
 ******************************************************************************/
sl_status_t sli_set_saturation(int level);

/***************************************************************************//**
 * @brief
 *  Internal driver function to set camera brightness.
 * 
 * @param[in] level 
 *  Integer value to set camera brightness[-2 to 2].
 * 
 * @return
 *  @ref sl_status_t SL_STATUS_OK for successful completion.
 ******************************************************************************/
sl_status_t sli_set_brightness(int level);

/***************************************************************************//**
 * @brief
 *  Internal driver function to set camera data format.
 * 
 * @param[in] format 
 *  Integer value to set camera data format.
 * 
 * @return
 *  @ref sl_status_t SL_STATUS_OK for successful completion.
 ******************************************************************************/
sl_status_t sli_set_format(int format);

/***************************************************************************//**
 * @brief
 *  Internal driver function to set camera gainceiling.
 * 
 * @param[in] gainceiling 
 *  Integer value to set camera gainceiling.
 * 
 * @return
 *  @ref sl_status_t SL_STATUS_OK for successful completion.
 ******************************************************************************/
sl_status_t sli_set_gainceiling(int gainceiling);

/***************************************************************************//**
 * @brief
 *  Internal driver function to set camera flip functionality.
 * 
 * @param[in] flip 
 *  Integer value to set camera flip functionality.
 * 
 * @return
 *  @ref sl_status_t SL_STATUS_OK for successful completion.
 ******************************************************************************/
sl_status_t sli_set_flip(int flip);

/***************************************************************************//**
 * @brief
 *  Internal driver function to set camera mirror image.
 * 
 * @param[in] mirror 
 *  Integer value to set camera mirror image.
 * 
 * @return
 *  @ref sl_status_t SL_STATUS_OK for successful completion.
 ******************************************************************************/
sl_status_t sli_set_mirror(int mirror);

/***************************************************************************//**
 * @brief
 *  Internal driver function to set camera secial effects.
 * 
 * @param[in] effect 
 *  Integer value to set camera special effect.
 * 
 * @return
 *  @ref sl_status_t SL_STATUS_OK for successful completion.
 ******************************************************************************/
sl_status_t sli_set_specialeffect(int effect);

/***************************************************************************//**
 * @brief
 *  Internal driver function to set camera frame size.
 * 
 * @param[in] resolution 
 *  @ref arducam_resolution_t resolution of camera.
 *
 * @param[in] width 
 *  Integer value to set camera image width.
 * 
 * @param[in] height 
 *  Integer value to set camera image height.
 * 
 * @return
 *  @ref sl_status_t SL_STATUS_OK for successful completion.
 ******************************************************************************/
sl_status_t sli_set_framesize(arducam_resolution_t resolution, uint32_t width, uint32_t height);
