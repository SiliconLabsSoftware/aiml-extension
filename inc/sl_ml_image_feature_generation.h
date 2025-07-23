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
#ifndef SL_ML_IMAGE_FEATURE_GENERATION_H
#define SL_ML_IMAGE_FEATURE_GENERATION_H

#include "sl_status.h"
#include "sl_component_catalog.h"
#include <stdint.h>
#include <stddef.h>

#if defined(SL_CATALOG_TFLITE_MICRO_PRESENT)
#include "tensorflow/lite/c/common.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @addtogroup ml_image_feature_generation Image Feature Generator
 * The image feature generator process input image captured from camera 
 * and extracts required features to use with machine learning image classification
 * applications using a camera as an image source.
 *
 * @details
 * ## Feature Generation

 * The captured image pre-processed using crop and mean-std normalization based
 * on model configuration used from application.
 *
 *                 Image
 *                   │
 *           ┌───────▼───────┐
 *           │      Crop     │
 *           └───────┬───────┘
 *                   │
 *           ┌───────▼───────┐
 *           │  Check mean   │
 *           └───────┬───────┘
 *                   │
 *           ┌───────▼───────┐
 *           │ Normalization │
 *           └───────┬───────┘
 *                   │
 *                   ▼
 *                 Model
 *
 * ##  Usage
 * sl_ml_image_feature_generation_init() initializes the feature generation
 * based on the configuration in sl_ml_image_feature_generation_config.h. It also
 * initializes and starts the camera in streaming mode, which places the image
 * samples into a ping-pong-buffer.
 *
 * The image is pre-processed and filled in input tensor when 
 * sl_ml_image_feature_generation_fill_tensor() is called.
 *
 * To retrieve the generated features
 * sl_ml_image_feature_generation_fill_tensor() must be called.
 *
 *
 * ### Example
 *
 * When used with TensorFlow Lite Micro, the image feature generator can be used to fill
 * a tensor directly by using sl_ml_image_feature_generation_fill_tensor(). However,
 * the model has to be trained using the same feature generator configurations as
 * used for inference, configured in sl_ml_image_feature_generation_config.h.
 *
 * ```
 * #include "sl_tflite_micro_init.h"
 * #include "sl_ml_image_feature_generation.h"
 *
 * void main(void)
 * {
 *   sl_ml_image_feature_generation_init();
 *
 *   while(1){ *
 *     if(do_inference){
 *       sl_ml_image_feature_generation_fill_tensor(sl_tflite_micro_get_input_tensor());
 *       sl_tflite_micro_get_interpreter()->Invoke();
 *     }
 *
 *     ...
 *
 *   }
 * }
 *
 * ```
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * @brief
 *    Set up the camera as an image source for feature generation.
 *
 * @return
 *    SL_STATUS_OK for success
 *    SL_STATUS_FAIL
 ******************************************************************************/
sl_status_t sl_ml_image_feature_generation_init();

/***************************************************************************//**
 * @brief
 *    Fill a TensorFlow tensor with feature data.
 *    Data type of input image for model will be selected based on model configuration.
 *
 * @param[in] input_tensor
 *    The input tensor to fill with features.
 *
 * @note
 *    This function overwrites the entire input tensor.
 *
 * @return
 *    SL_STATUS_OK for success
 *    SL_STATUS_INVALID_PARAMETER Tensor type or size does not correspond with
 *    configuration
 ******************************************************************************/
sl_status_t sl_ml_image_feature_generation_fill_tensor(TfLiteTensor* input_tensor);

/***************************************************************************//**
 * @brief
 *    Set up the aurducam as an image source for feature generation and
 *    initialize the camera with configuration.
 *
 * @return
 *    SL_STATUS_OK for success
 *    SL_STATUS_FAIL
 ******************************************************************************/
sl_status_t sl_ml_initialize_arducam_camera();

/***************************************************************************//**
 * @brief
 *    Set up the JLink stream as output node. Image data streamed over JLink
 *    to python interface.
 *
 * @return
 *    SL_STATUS_OK for success
 *    SL_STATUS_FAIL
 ******************************************************************************/
void sl_ml_dump_image(const uint8_t* image_data, uint32_t image_length);

/***************************************************************************//**
 * @brief
 *    Retrieve image from ping-pog buffer captured from aurducam.
 *    Store image data to destination pointer.
 *
 ******************************************************************************/
void sl_ml_retrieve_next_camera_image(uint8_t** image_data, uint32_t *image_size);

/***************************************************************************//**
 * @brief
 *    Initializes Look up table for croping image.
 *
 ******************************************************************************/
void sl_ml_image_crop_lut_init();

/***************************************************************************//**
 * @brief
 *    Retrieve the features, scales them by the given scaler, and fills float buffer.
 *
 *    buffer = (float)image_data * scaler

 * @param[out] buffer
 *    Pointer to the buffer to store the scaled feature data
 *
 * @param[in] num_elements
 *    The number of elements corresponding to the size of the buffer; If this is
 *    not large enough to store the entire feature buffer the function will return
 *    with an error.
 *
 * @param[in] scaler
 *    The scaling factor to apply to each feature value.
 *
 * @note
 *    This function overwrites the entire buffer.
 *
 * @return
 *    SL_STATUS_OK for success
 *    SL_STATUS_INVALID_PARAMETER num_elements too small
 ******************************************************************************/
sl_status_t sl_ml_image_feature_generation_get_image_scaled(uint8_t* image_data, float *buffer, size_t num_elements, float scaler);

/***************************************************************************//**
 * @brief
 *    Computes mean and mean square of input image.
 *
 *    mean = ((float)image_data / (width*length)
 *
 * @param[out] mean
 *    Pointer to the mean to store the mean value of image.
 *
 * @param[out] mean2
 *    Pointer to the mean2 to store the mean square value of image.
 *
 * @param[in] num_elements
 *    The number of elements corresponding to the size of the buffer; If this is
 *    not large enough to store the entire feature buffer the function will return
 *    with an error.
 *
 * @return
 *    SL_STATUS_OK for success
 *    SL_STATUS_INVALID_PARAMETER num_elements too small
 ******************************************************************************/
sl_status_t sl_ml_get_image_mean(uint8_t *in_cam_image, float *mean, float *mean2, size_t num_elements);

/***************************************************************************//**
 * @brief
 *    Retrieve the features, normalizes them by centering about their mean and standard deviation
 *    and fills float buffer.
 *
 *    buffer = ((float)image_data - mean(image_data)) / std(image_data)

 * @param[out] buffer
 *    Pointer to the buffer to store the normalized feature data
 *
 * @param[in] mean
 *    float mean, mean value of image.
 *
 * @param[in] mean2
 *    float mean2, mean square value of image.
 *
 * @param[in] num_elements
 *    The number of elements corresponding to the size of the buffer; If this is
 *    not large enough to store the entire feature buffer the function will return
 *    with an error.
 *
 * @note
 *    This function overwrites the entire buffer.
 *
 * @return
 *    SL_STATUS_OK for success
 *    SL_STATUS_INVALID_PARAMETER num_elements too small
 ******************************************************************************/
sl_status_t sl_ml_image_feature_generation_get_image_mean_std_normalized(uint8_t* image_data, float *buffer,float mean, float mean2, size_t num_elements);

/***************************************************************************//**
 * @brief
 *    Retrieve the features quantized to signed integer numbers in the range
 *    -128 to 127 (int8) and copy them to the provided buffer.
 *
 *    @ref range_min and @ref range_max set the valid range of which values are
 *    quantized from.
 *
 * @param[out] buffer
 *    Pointer to the buffer to store the feature data
 *
 * @param[in] num_elements
 *    The number of elements corresponding to the size of the buffer, if this is
 *    not large enough to store the entire feature buffer the function will return
 *    with an error.
 *
 * @param[in] range_min
 *    Determines the lower point of the range to quantize features from
 *
 * @param[in] range_max
 *    Determines the largest point of the range to quantize features from
 *
 * @note
 *    This function overwrites the entire buffer.
 *
 * @return
 *    SL_STATUS_OK for success
 *    SL_STATUS_INVALID_PARAMETER invalid input parameters
 ******************************************************************************/
sl_status_t sli_ml_image_feature_generation_get_image_quantized(uint8_t* image_data, int8_t *buffer,
                                                                size_t num_elements);

/***************************************************************************//**
 * @brief
 *    Retrieve the features as type float32 and copy them to the provided buffer.
 *
 * @param[out] buffer
 *    Pointer to the buffer to store the feature data
 *
 * @param[in] num_elements
 *    The number of elements corresponding to the size of the buffer; If this is
 *    not large enough to store the entire feature buffer the function will return
 *    with an error.
 *
 * @note
 *    This function overwrites the entire buffer.
 *
 * @return
 *    SL_STATUS_OK for success
 *    SL_STATUS_INVALID_PARAMETER num_elements too small
 ******************************************************************************/
sl_status_t sl_ml_image_feature_generation_get_image_raw_float32(uint8_t* image_data, float *buffer, size_t num_elements);

/***************************************************************************//**
 * @brief
 *    Reset the state of the image feature generator.
 ******************************************************************************/
void sl_ml_image_feature_generation_reset();
/** @} (end addtogroup ml_image_feature_generation) */

#ifdef __cplusplus
}
#endif

#endif // SL_ML_IMAGE_FEATURE_GENERATION_H
