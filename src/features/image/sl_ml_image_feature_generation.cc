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
#include <stdlib.h>
#include "sl_status.h"
#include "sl_ml_image_feature_generation.h"
#include "sl_ml_image_feature_generation_config.h"
#include "sl_common.h"
#include "sl_sleeptimer.h"
#include "math.h"
#include "sl_ml_arducam.h"
#include "sl_ml_jlink_stream.hpp"

/*******************************************************************************
 *********************************   DEFINES   *********************************
 ******************************************************************************/
// Model input image length
#define SL_ML_MODEL_INPUT_IMG_BUFFER_LENGTH     (SL_ML_MODEL_INPUT_WIDTH * SL_ML_MODEL_INPUT_HEIGHT)
// Camera image data length
#define SL_ML_CAM_IMG_LENGTH     (SL_ML_CAM_IMAGE_HEIGHT * SL_ML_CAM_IMAGE_WIDTH)
//  RGB 565 buffer needs 2 bytes per pixel
// Ping-pong buffer requires 2 buffer count
#define SL_ML_CAM_INPUT_IMG_BUFFER_LENGTH     (SL_ML_CAM_IMG_LENGTH * 2 * 2)
/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/

//  Buffer camera image
static uint8_t image_buffer[SL_ML_CAM_INPUT_IMG_BUFFER_LENGTH];

// Image crop buffer 
static uint32_t crop_buffer[SL_ML_MODEL_INPUT_IMG_BUFFER_LENGTH];

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 *  Fills a TensorFlow tensor with feature data
 ******************************************************************************/
sl_status_t sl_ml_image_feature_generation_fill_tensor(TfLiteTensor *input_tensor)
{
  uint8_t* cam_image_data;
  uint32_t image_size;
  // Retrieve the image from the camera
  sl_ml_retrieve_next_camera_image(&cam_image_data, &image_size);
  // This will also dump the image to the JLink stream if necessary
  sl_ml_dump_image(cam_image_data, image_size);
  // Convert color space to gray
  sl_ml_convert_rgb565_to_gray(cam_image_data);
  
  sl_status_t status = SL_STATUS_OK;
  float mean = 0.0f;
  float mean2 = 0.0f;
  if (SL_ML_IMAGE_FEATURE_GENERATION_SAMPLEWISE_NORM_MEAN_AND_STD || (SL_ML_IMAGE_MEAN_THRESHOLD != 0))
  {
    const int n_elements = input_tensor->bytes / sizeof(float);
    sl_ml_get_image_mean(cam_image_data, &mean, &mean2,  n_elements);
  }

  if ((SL_ML_IMAGE_MEAN_THRESHOLD != 0) && (mean < SL_ML_IMAGE_MEAN_THRESHOLD))
  {
      printf("Image brightness if not sufficient. Skipping frame\n.");
      slx_ml_arducam_release_image();
      return SL_STATUS_INVALID_RANGE;
  }

  if (input_tensor->type == kTfLiteInt8) {
      status = sli_ml_image_feature_generation_get_image_quantized(
        cam_image_data,
        input_tensor->data.int8,
        input_tensor->bytes
      );
  } else if(input_tensor -> type == kTfLiteFloat32) {
    const int n_elements = input_tensor->bytes / sizeof(float);

    if(SL_ML_IMAGE_FEATURE_GENERATION_SAMPLEWISE_NORM_RESCALE != 0) {
      status = sl_ml_image_feature_generation_get_image_scaled(
        cam_image_data,
        input_tensor->data.f,
        n_elements,
        SL_ML_IMAGE_FEATURE_GENERATION_SAMPLEWISE_NORM_RESCALE
      );
    } else if(SL_ML_IMAGE_FEATURE_GENERATION_SAMPLEWISE_NORM_MEAN_AND_STD) {
      status = sl_ml_image_feature_generation_get_image_mean_std_normalized(
        cam_image_data,
        input_tensor->data.f,
        mean,
        mean2,
        n_elements
      );
    } else {
      status = sl_ml_image_feature_generation_get_image_raw_float32(
        cam_image_data,
        input_tensor->data.f,
        n_elements
      );
    }

  } else {
    status = SL_STATUS_INVALID_PARAMETER;
  }
  slx_ml_arducam_release_image();

  return status;
}

void sl_ml_image_feature_generation_reset()
{
  // Clear buffers
  for (int i = 0; i < SL_ML_MODEL_INPUT_IMG_BUFFER_LENGTH; i++) {
    crop_buffer[i] = 0;
  }
  for (int i = 0; i < SL_ML_CAM_INPUT_IMG_BUFFER_LENGTH; i++) {
    image_buffer[i] = 0;
  }
  // De-init arducam
  slx_ml_arducam_deinit();
}

/*******************************************************************************
 **************************   LOCAL FUNCTIONS   ********************************
 ******************************************************************************/

 /***************************************************************************//**
 *  Function initializes arducam camera
 ******************************************************************************/
sl_status_t sl_ml_initialize_arducam_camera()
{
    sl_status_t status;

    // Initialize the camera
    arducam_config_t cam_config = ARDUCAM_DEFAULT_CONFIG;
    cam_config.image_resolution.width = SL_ML_CAM_IMAGE_WIDTH;//input_shape->data[2];//input_shape[2];
    cam_config.image_resolution.height = SL_ML_CAM_IMAGE_HEIGHT;//input_shape->data[1];//input_shape[1];
    cam_config.data_format = ARDUCAM_DATA_FORMAT_RGB565; //ARDUCAM_DATA_FORMAT_GRAYSCALE

    // Initialize the camera
    status = slx_ml_arducam_init(&cam_config, image_buffer, SL_ML_CAM_INPUT_IMG_BUFFER_LENGTH);
    if(status != SL_STATUS_OK)
    {
        printf("Failed to initialize the camera\n");
        return status;
    }

   // Start the image capturing DMA background
    status = slx_ml_arducam_start_capture();
    if(status != SL_STATUS_OK)
    {
        printf("Failed to start camera capture\n");
        return status;
    }

    return SL_STATUS_OK;
}

/***************************************************************************//**
 * Dump camera image to JLink stream
 ******************************************************************************/
void sl_ml_dump_image(const uint8_t* image_data, uint32_t image_length)
{
    bool connected = false;

    // Check if the Python script has connected
    slx_ml_jlink_stream::slx_ml_is_connected("image", &connected);
    if(connected)
    {
     slx_ml_jlink_stream::slx_ml_write_all("image", image_data, image_length);
    }
}

/***************************************************************************//**
 * Retrieve the next image from the camera
 ******************************************************************************/
void sl_ml_retrieve_next_camera_image(uint8_t** image_data, uint32_t *image_size)
{

    for(;;)
    {
        sl_status_t status = slx_ml_arducam_get_next_image(image_data, image_size);

        if(status == SL_STATUS_IN_PROGRESS)
        {
            // NOTE: Unfortunately, the camera doesn't not have a way to interrupt the MCU
            //       once an image is ready. So we must periodically poll the camera to check
            //       its status. This can be done by continuously calling arducam_get_next_image()
            //       OR arducam_poll() from a timer interrupt handler.
            sl_sleeptimer_delay_millisecond(5);
            continue;
        }
        else if(status != SL_STATUS_OK)
        {
            printf("ERROR: Failed to retrieve image\n");
            while (1)
            ;
        }
        break;
    }
    
}

/***************************************************************************//**
 *  Initialize LUT for crop image to required resolution.
 ******************************************************************************/
void sl_ml_image_crop_lut_init()
{

  uint32_t i_store_index, i_dst_index;
  // COmpute center point location in camera image
  uint16_t i_center_x = (int16_t)(SL_ML_CAM_IMAGE_WIDTH/2);
  uint16_t i_center_y = (int16_t)(SL_ML_CAM_IMAGE_HEIGHT/2);
  // Compute starting point location on camera image
  uint16_t i_start_x = (int16_t)(i_center_x - SL_ML_MODEL_INPUT_WIDTH/2);
  uint16_t i_start_y = (int16_t)(i_center_y - SL_ML_MODEL_INPUT_HEIGHT/2);
  // compute end point location on camera image
  uint16_t i_end_x = (int16_t)(i_center_x + SL_ML_MODEL_INPUT_WIDTH/2);
  uint16_t i_end_y = (int16_t)(i_center_y + SL_ML_MODEL_INPUT_HEIGHT/2);

  for (uint16_t i_row = i_start_y; i_row < i_end_y; i_row++)
  {
    for (uint16_t i_col = i_start_x; i_col < i_end_x; i_col++)
    {
        // Compute crop index from camera resolution
        i_store_index = i_col +i_row*SL_ML_CAM_IMAGE_WIDTH;
        // compute index for buffer
        i_dst_index = (i_col-i_start_x) +(i_row-i_start_y)*SL_ML_MODEL_INPUT_WIDTH;
        crop_buffer[i_dst_index] = i_store_index;

    }
  }
}

/***************************************************************************//**
 * buffer = (float)image_data / scaler
 ******************************************************************************/
sl_status_t sl_ml_image_feature_generation_get_image_scaled(uint8_t *in_cam_image, float *buffer, size_t num_elements, float scaler)
{
  if ((int)num_elements != SL_ML_MODEL_INPUT_IMG_BUFFER_LENGTH) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (scaler == 0) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  uint32_t read_index;
  for (int i = 0; i < SL_ML_MODEL_INPUT_IMG_BUFFER_LENGTH; i++) {
    read_index = crop_buffer[i];
    const float value = (float)in_cam_image[read_index];
    buffer[i] = value * scaler;
  }

  return SL_STATUS_OK;
}
/***************************************************************************//**
 *  mean = sum(image_data)) / (width*height)
 ******************************************************************************/
sl_status_t sl_ml_get_image_mean(uint8_t *in_cam_image, float *mean, float *mean2, size_t num_elements)
{
  uint32_t read_index;
  float count = 0.0f;

  if ((int)num_elements != SL_ML_MODEL_INPUT_IMG_BUFFER_LENGTH) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  // Calculate the mean and mean square
  for(int i = SL_ML_MODEL_INPUT_IMG_BUFFER_LENGTH; i > 0; --i)
  {
    read_index = crop_buffer[i];
    const float value = (float)in_cam_image[read_index];
    count += 1;
    const float delta = value - *mean;
    *mean += delta / count;
    const float delta2 = value - *mean;
    *mean2 += delta * delta2;
  }
  return SL_STATUS_OK;
}
 /***************************************************************************//**
 *  buffer = ((float)image_data - mean(image_data)) / std(image_data)
 ******************************************************************************/
sl_status_t sl_ml_image_feature_generation_get_image_mean_std_normalized(uint8_t *in_cam_image, float *buffer,float mean, float mean2, size_t num_elements)
{
  if ((int)num_elements != SL_ML_MODEL_INPUT_IMG_BUFFER_LENGTH) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  uint32_t read_index;

  // Calculate the STD
  const float variance = mean2 / (float)num_elements;
  const float std = sqrtf(variance);
  const float std_recip = 1.0f / std; // multiplication is faster than division

  // Subtract the mean and divide by the STD
  float* dst = buffer;
  for(int i =0;i< SL_ML_MODEL_INPUT_IMG_BUFFER_LENGTH; i++)
  {
    read_index = crop_buffer[i];
    const float value = (float)in_cam_image[read_index];
    const float x = value - mean;
    *dst++ = x * std_recip;
  }

  return SL_STATUS_OK;
}


/***************************************************************************//**
 * @brief
 *    Retrieves the features quantized to signed integer numbers in the range
 *    -128 to 127 (int8) and copies them to the provided buffer.
 *
 *    @ref range_min and @ref range_max set the valid range of which values are
 *    quantized from.
 ******************************************************************************/
sl_status_t sli_ml_image_feature_generation_get_image_quantized(uint8_t *in_cam_image,
                                                                int8_t *buffer,
                                                                size_t num_elements
                                                              )
{
  if ((int)num_elements != SL_ML_MODEL_INPUT_IMG_BUFFER_LENGTH) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  uint32_t read_index;
  int32_t value;
  for (int i = 0; i < SL_ML_MODEL_INPUT_IMG_BUFFER_LENGTH; i++) {
    read_index = crop_buffer[i];
    value = in_cam_image[read_index];
    value -= 128;
    if (value < -128) {
      value = -128;
    }
    if (value > 127) {
      value = 127;
    }
    buffer[i] = (int8_t)value;
  }
  return SL_STATUS_OK;
}

/***************************************************************************//**
 *  Retrieves the image as type float and copies them to the provided buffer.
 ******************************************************************************/
sl_status_t sl_ml_image_feature_generation_get_image_raw_float32(uint8_t *in_cam_image, float *buffer, size_t num_elements)
{
  if ((int)num_elements != SL_ML_MODEL_INPUT_IMG_BUFFER_LENGTH) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  uint32_t read_index;
  for (int i = 0; i < SL_ML_MODEL_INPUT_IMG_BUFFER_LENGTH; i++) {
    read_index = crop_buffer[i];
    buffer[i] = (float)in_cam_image[read_index];
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 *  Function to convert image color space RGB565 to gray.
 ******************************************************************************/
void sl_ml_convert_rgb565_to_gray(uint8_t* img)
{
    const uint8_t *src = img;
    uint8_t *dst = img;

    for(uint32_t pixel_count = SL_ML_CAM_IMG_LENGTH; pixel_count > 0; --pixel_count)
    {
        uint16_t lb = src[0];
        uint16_t hb = src[1];
        uint16_t rgb565 = lb + (hb<<8);
        uint8_t r = (((rgb565 & 0xF800) >> 11) << 3);
        uint8_t g = (((rgb565 & 0x7E0) >> 5) << 2);
        uint8_t b = (((rgb565 & 0x1F)) << 3);
        //weighted rgb to grayscale conversion
        uint16_t val = ((0.2126 * r) + (0.7152 * g ) + (0.0722 * b));
        *dst++ = (val <= 255) ? (uint8_t)val : (uint8_t)255;
        src += 2;
    }
}
