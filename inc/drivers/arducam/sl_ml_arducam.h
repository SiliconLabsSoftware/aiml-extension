
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

#include "sl_status.h"
#include "sl_ml_arducam_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @brief
 *  Function initializes arducam camera driver.
 *
 * @param[in] config
 *  @ref arducam_config_t driver configuration
 * 
 * @param[in] image_buffer
 *  Pointer to image data buffer
 * 
 * @param[in] buffer_length
 *  Size of the image data buffer in bytes, this should be a multiple of 
 *  @ref arducam_calculate_image_buffer_length()
 *
 * @return
 *  @ref sl_status_t SL_STATUS_OK for successful initialization of camera driver,
 *  SL_STATUS_NOT_SUPPORTED When camera not supported with available driver sw to intialize camera.
 ******************************************************************************/
sl_status_t slx_ml_arducam_init(const arducam_config_t* config, uint8_t* image_buffer, uint32_t buffer_length);

/***************************************************************************//**
 * @brief
 *  Function De-initialize arducam camera diver.
 *
 * @return
 *  @ref sl_status_t SL_STATUS_OK when suceed.
 ******************************************************************************/
sl_status_t slx_ml_arducam_deinit();

/***************************************************************************//**
 * @brief
 *  Function sets arducam camera setting.
 *
 * @param[in] setting
 *  @ref arducam_setting_t Setting to set
 * @param[in] value
 *  Value of the setting
 *
 * @note Settings may be set while capturing is active.
 * 
 * @return
 *  @ref sl_status_t SL_STATUS_OK when succeed.
 ******************************************************************************/
sl_status_t slx_ml_arducam_set_setting(arducam_setting_t setting, int32_t value);

/***************************************************************************//**
 * @brief
 *  Function used to start image capturing.
 *
 *  This enables the camera to begin capturing images.
 *  After calling this, periodically call @ref slx_ml_arducam_get_next_image() to retrieve a pointer
 *  to the next available image.
 *  Call @ref slx_ml_arducam_stop_capture() to disable capturing.
 * 
 * @note 
 *  @ref slx_ml_arducam_init() must be called before using this API.
 * 
 * @return
 *  @ref sl_status_t SL_STATUS_OK when succeed
 ******************************************************************************/
sl_status_t slx_ml_arducam_start_capture();

/***************************************************************************//**
 * @brief
 *  Function used to stop image capturing.
 *  This disables the camera from capturing images.
 * 
 * @return
 *  @ref sl_status_t SL_STATUS_OK when succeed
 ******************************************************************************/
sl_status_t slx_ml_arducam_stop_capture();

/***************************************************************************//**
 * @brief
 *  Function used to Poll the camera.
 * 
 * @note This API is optional. It can help improve throughput when
 *  periodically called from a timer interrupt.
 * 
 * @return
 *  @ref sl_status_t SL_STATUS_OK when succeed
 ******************************************************************************/
sl_status_t slx_ml_arducam_poll();

/***************************************************************************//**
 * @brief
 *  Attempt to retrieve the next capturing image.
 *    
 *  This attempts to retrieve the next capturing image from the camera.
 *  If an image is available, the data_ptr argument will point to the image buffer
 *  and the length_ptr will contain the length of the image in bytes.
 * 
 * @note 
 *  @ref slx_ml_arducam_release_image() MUST be called once the image is no longer
 *  used. This API must NOT be called again until the previous image is released.
 *  @ref slx_ml_arducam_start_capture() must be called before using this API.
 * 
 * @param[out] data_ptr Pointer to hold reference to next captured image, 
 *  NULL if no image is available
 * 
 * @param[out] length_ptr Optional pointer to hold length of image in bytes. 
 *  Leave NULL if unused.
 * 
 * @return
 *  @ref sl_status_t SL_STATUS_OK when succeed
 *  SL_STATUS_IN_PROGRESS if no image is currently available.
 ******************************************************************************/
sl_status_t slx_ml_arducam_get_next_image(uint8_t** data_ptr, uint32_t* length_ptr);

/***************************************************************************//**
 * @brief
 *  Releases previous image from buffer.
 *  This releases an image previously returned by @ref slx_ml_arducam_get_next_image().
 * 
 * @note 
 *  This MUST be called once the image returned by  @ref slx_ml_arducam_get_next_image()
 *  is no longer used.
 * 
 * @return
 *  @ref sl_status_t SL_STATUS_OK when succeed
 ******************************************************************************/
sl_status_t slx_ml_arducam_release_image();

/***************************************************************************//**
 * @brief
 *  Return number bytes per image
 *  This returns the numbers of bytes per image used by the image buffer.
 * 
 * @param[in] format 
 *  @ref arducam_data_format_t Image data format
 * 
 * @param[in] width 
 *  Width of image in pixels
 * 
 * @param[in] height 
 *  Height of image in pixels
 * 
 * @note This value may be different than the value returned by @ref slx_ml_arducam_calculate_image_size()
 * due to internal color space conversion requirements.
 * 
 * @return
 *  @ref uint32_t Bytes required given image parameters
 ******************************************************************************/
uint32_t slx_ml_arducam_calculate_image_buffer_length(arducam_data_format_t format, uint32_t width, uint32_t height);

/***************************************************************************//**
 * @brief
 *  Return size of image in pixels
 *  This returns the size of the image in bytes.
 * 
 * @param[in] format 
 *  @ref arducam_data_format_t Image data format
 * 
 * @param[in] width 
 *  Width of image in pixels
 * 
 * @param[in] height 
 *  Height of image in pixels
 * 
 * @note This value may be different than the value returned by @ref slx_ml_arducam_calculate_image_size()
 * due to internal color space conversion requirements.
 * 
 * @return
 *  @ref uint32_t Size of image in pixels
 ******************************************************************************/
uint32_t slx_ml_arducam_calculate_image_size(arducam_data_format_t format, uint32_t width, uint32_t height);

 /***************************************************************************//**
 *  Local Functions
 ******************************************************************************/

/***************************************************************************//**
 * @brief
 *  This function increments buffer pointer by image size in bytes.
 * 
 * @param[out] ptr 
 *  Pointer holding reference to image data. 
 ******************************************************************************/
void sli_increment_buffer_pointer(uint8_t** ptr);

/***************************************************************************//**
 * @brief
 *  Function to convert image color space.
 * 
 * @param[out] img 
 *  Pointer holding reference to image data. 
 ******************************************************************************/
void sli_convert_image_color_space(uint8_t* img);

#ifdef __cplusplus
}
#endif
