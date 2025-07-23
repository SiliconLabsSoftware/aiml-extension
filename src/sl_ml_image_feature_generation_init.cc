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
  // Initializing jlink stream for writing
  slx_ml_jlink_stream::slx_ml_register_stream("image", slx_ml_jlink_stream::Write);
  
  return SL_STATUS_OK;
}
