/***************************************************************************//**
 * @file
 * @brief Silicon Labs Feature Generation Initialization with image source
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
#include <stdio.h>
#include "sl_ml_arducam.h"
#include "sl_ml_image_feature_generation_config.h"
#include "sl_ml_image_feature_generation.h"
#include "sl_ml_jlink_stream.hpp"


/***************************************************************************//**
 *  Retrieves the features as type uint16 and copies them to the provided buffer.
 ******************************************************************************/
sl_status_t sl_ml_image_feature_generation_init()
{
  // Initialize camera
  sl_status_t status;
  // Initializing arducam camera.
  status = sl_ml_initialize_arducam_camera();
  //If status fails to initiate, reset feature generation model.
  if (status != SL_STATUS_OK) {
    sl_ml_image_feature_generation_reset();
    return status;
  }
  // JLink image stream is optional: failure is WARNING-only; inference and return value
  // still reflect successful camera init (SL_STATUS_OK).
  const sl_status_t jlink_status =
      slx_ml_jlink_stream::slx_ml_register_stream("image", slx_ml_jlink_stream::Write);
  if (jlink_status != SL_STATUS_OK) {
    printf("WARNING: Failed to register JLink 'image' stream "
           "(sl_status_t=0x%04lX).\n"
           "  Image visualization will be unavailable.\n"
           "  Possible causes:\n"
           "    1. malloc failed - heap too small for ~8 KB stream buffer.\n"
           "       Current image_buffer = %u bytes "
           "(cam %dx%d, 2 bytes/px, %d ping-pong).\n"
           "       Reduce SL_ML_CAM_IMAGE_WIDTH/HEIGHT to free heap.\n"
           "    2. Stream 'image' already registered (duplicate init).\n",
           (unsigned long)jlink_status,
           (unsigned)(SL_ML_CAM_IMAGE_WIDTH * SL_ML_CAM_IMAGE_HEIGHT
                      * 2 * SL_ML_IMAGE_BUFFER_COUNT),
           SL_ML_CAM_IMAGE_WIDTH, SL_ML_CAM_IMAGE_HEIGHT,
           SL_ML_IMAGE_BUFFER_COUNT);
  }

  return SL_STATUS_OK;
}
