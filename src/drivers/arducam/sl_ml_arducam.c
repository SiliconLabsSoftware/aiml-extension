/***************************************************************************//**
 * @file
 * @brief Silicon Labs Arducam camera peripheral driver.
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
#include <stdbool.h>

#include "em_device.h"
#include "em_core.h"

#include "sl_ml_arducam.h"
#include "sl_ml_arducam_m_2mp_driver.h"

arducam_driver_context_t arducam_context;

 /***************************************************************************//**
 *  Function initializes arducam camera driver
 ******************************************************************************/
sl_status_t slx_ml_arducam_init(const arducam_config_t* config, uint8_t* image_buffer, uint32_t image_buffer_length)
{
    sl_status_t status;

    if(arducam_context.is_initialized)
    {
        return SL_STATUS_ALREADY_INITIALIZED;
    }

    arducam_context.data_format = config->data_format;
    arducam_context.image_size_bytes = slx_ml_arducam_calculate_image_size(
        config->data_format, 
        config->image_resolution.width, 
        config->image_resolution.height
    );

    const uint32_t buffer_bytes_per_image = slx_ml_arducam_calculate_image_buffer_length(
        config->data_format, 
        config->image_resolution.width, 
        config->image_resolution.height
    );
    arducam_context.buffer.buffer_bytes_per_image = buffer_bytes_per_image;


    arducam_context.buffer.max_local_count = (image_buffer_length / buffer_bytes_per_image);
    if(arducam_context.buffer.max_local_count == 0)
    {
        return SL_STATUS_INVALID_PARAMETER;
    }

    arducam_context.buffer.start = image_buffer;
    arducam_context.buffer.end = image_buffer + arducam_context.buffer.max_local_count * buffer_bytes_per_image;

    // This camera returns either YUV422 or RGB565 images
    // Both formats require 2 bytes per pixel
    // NOTE: grayscale uses RGB565
    arducam_context.buffer.read_length = config->image_resolution.width * config->image_resolution.height * 2;

    // If we want to return an RGB88 image 
    // then we need to expand the RGB565 image in-place.
    // We do this by right-justifying the RGB565 image in the buffer
    if(config->data_format == ARDUCAM_DATA_FORMAT_RGB888)
    {
        arducam_context.buffer.read_buffer_offset = buffer_bytes_per_image - arducam_context.buffer.read_length;
    }

    status = slx_ml_arducam_driver_init(config);
    if(status != SL_STATUS_OK)
    {
        return status;
    }

    arducam_context.is_initialized = true;

    return SL_STATUS_OK;
}

 /***************************************************************************//**
 *  Function de-initializes arducam camera driver
 ******************************************************************************/
sl_status_t slx_ml_arducam_deinit()
{
    if(!arducam_context.is_initialized)
    {
        return SL_STATUS_NOT_INITIALIZED;
    }

    slx_ml_arducam_stop_capture();

    slx_ml_arducam_driver_deinit();

    arducam_context.is_initialized = false;

    return SL_STATUS_OK;
}

 /***************************************************************************//**
 *  To adjust arducam settings like brightness, contrast
 ******************************************************************************/
sl_status_t slx_ml_arducam_set_setting(arducam_setting_t setting, int32_t value)
{
    if(!arducam_context.is_initialized)
    {
        return SL_STATUS_NOT_INITIALIZED;
    }

    return slx_ml_ov2640_set_setting(setting, value);
}

 /***************************************************************************//**
 *  Starts capturing images and store in buffer
 ******************************************************************************/
sl_status_t slx_ml_arducam_start_capture()
{
    CORE_DECLARE_IRQ_STATE;

    if(!arducam_context.is_initialized)
    {
        return SL_STATUS_NOT_INITIALIZED;
    }

    CORE_ENTER_CRITICAL();

    if(arducam_context.is_started)
    {
        CORE_EXIT_CRITICAL();
        return SL_STATUS_OK;
    }

    arducam_context.is_image_locked = false;
    arducam_context.buffer.head = arducam_context.buffer.tail = arducam_context.buffer.start;
    arducam_context.buffer.local_count = 0;
    arducam_context.is_started = true;
    arducam_context.state = CAMERA_STATE_IDLE;

    CORE_EXIT_CRITICAL();

    return slx_ml_arducam_poll();
}


 /***************************************************************************//**
 *  Stops image capture from arducam
 ******************************************************************************/
sl_status_t slx_ml_arducam_stop_capture()
{
    CORE_DECLARE_IRQ_STATE;

    CORE_ENTER_CRITICAL();

    if(!arducam_context.is_started)
    {
        CORE_EXIT_CRITICAL();
        return SL_STATUS_OK;
    }

    arducam_context.is_started = false;

    CORE_EXIT_CRITICAL();

    slx_ml_arducam_poll();

    return SL_STATUS_OK;
}

 /***************************************************************************//**
 *  Function used to Poll the camera.
 ******************************************************************************/
sl_status_t slx_ml_arducam_poll()
{
    CORE_DECLARE_IRQ_STATE;
    uint8_t spi_fifo_reg = 0;
    uint8_t* read_ptr = NULL;

    if(!arducam_context.is_initialized)
    {
        return SL_STATUS_NOT_INITIALIZED;
    }

    CORE_ENTER_CRITICAL();


    if(arducam_context.is_started && arducam_context.state == CAMERA_STATE_CAPTURING)
    {
        uint8_t camera_status_flags = 0;

        CORE_EXIT_CRITICAL();

        // Then poll the camera to see if the next image has been buffered in the camera's FIFO
        slx_ml_arducam_driver_spi_read_reg(ARDUCHIP_REG_STATUS, &camera_status_flags);

        CORE_ENTER_CRITICAL();

        if(camera_status_flags & CAP_DONE_MASK)
        {
            ARDUCAM_DEBUG("Capture complete");
            arducam_context.state = CAMERA_STATE_CAPTURE_COMPLETE;
        }
    }


    // If captruing is not started then just return
    if(!arducam_context.is_started)
    {
        CORE_EXIT_CRITICAL();
        return SL_STATUS_INVALID_STATE;
    }


    if(arducam_context.state == CAMERA_STATE_CAPTURE_COMPLETE)
    {
        if(arducam_context.buffer.local_count < arducam_context.buffer.max_local_count)
        {
            ARDUCAM_DEBUG("Start reading");
            arducam_context.state = CAMERA_STATE_READING;
            spi_fifo_reg = FIFO_RDPTR_RST_MASK;
            read_ptr = arducam_context.buffer.head + arducam_context.buffer.read_buffer_offset;
        }  
        else 
        {
            ARDUCAM_DEBUG("Local buffer full");
        }
    }

    if(arducam_context.state == CAMERA_STATE_READ_COMPLETE)
    {
        ARDUCAM_DEBUG("Read complete");
        // Increment to the next buffer in the local FIFO
        sli_increment_buffer_pointer(&arducam_context.buffer.head);

        // Increment the local buffer count
        ++arducam_context.buffer.local_count;
        arducam_context.state = CAMERA_STATE_IDLE;
    }

    if(arducam_context.state == CAMERA_STATE_IDLE)
    {
        ARDUCAM_DEBUG("Start capturing");
        spi_fifo_reg = FIFO_CLEAR_MASK|FIFO_WRPTR_RST_MASK|FIFO_START_MASK;
        arducam_context.state = CAMERA_STATE_CAPTURING;
    }


    CORE_EXIT_CRITICAL();

    // If we have REG_FIFO bits to set
    if(spi_fifo_reg != 0)
    {
        slx_ml_arducam_driver_spi_write_reg(ARDUCHIP_REG_FIFO, spi_fifo_reg);
    }

    // If we should begin reading another image from the camera
    if(read_ptr != NULL)
    {
        // uint32_t length;
        // arducam_driver_get_fifo_size(&length);
        // ARDUCAM_DEBUG("FIFO length: %d", length);
        slx_ml_arducam_driver_spi_burst_read(read_ptr, arducam_context.buffer.read_length);
    }

    return SL_STATUS_OK;
}

 /***************************************************************************//**
 *  Attempt to retrieve the next capturing image.
 ******************************************************************************/
sl_status_t slx_ml_arducam_get_next_image(uint8_t** data_ptr, uint32_t* length_ptr)
{
    CORE_DECLARE_IRQ_STATE;
    sl_status_t status = slx_ml_arducam_poll();

    if(status != SL_STATUS_OK)
    {
        return status;
    }
    else if(arducam_context.is_image_locked)
    {
        return SL_STATUS_INVALID_STATE;
    }

    CORE_ENTER_CRITICAL();

    if(arducam_context.buffer.local_count > 0)
    {
        ARDUCAM_DEBUG("Retrieve image");
        arducam_context.is_image_locked = true;
        *data_ptr = arducam_context.buffer.tail;
        if(length_ptr != NULL)
        {
            *length_ptr = arducam_context.image_size_bytes;
        }
        status = SL_STATUS_OK;
    }
    else
    {
        *data_ptr = NULL;
        if(length_ptr != NULL)
        {
            *length_ptr = 0;
        }
        status = SL_STATUS_IN_PROGRESS;
    }

    CORE_EXIT_CRITICAL();

    // Convert the image's color space in-place if necessary
    if(*data_ptr != NULL)
    {
        sli_convert_image_color_space(*data_ptr);
    }

    return status;
}

 /***************************************************************************//**
 *  Releases previous image from buffer.
 ******************************************************************************/
sl_status_t slx_ml_arducam_release_image()
{
    CORE_DECLARE_IRQ_STATE;
    sl_status_t status;

    if(!arducam_context.is_initialized)
    {
        return SL_STATUS_NOT_INITIALIZED;
    }
    else if(!arducam_context.is_image_locked)
    {
        return SL_STATUS_INVALID_STATE;
    }

    CORE_ENTER_CRITICAL();

    if(arducam_context.buffer.local_count > 0)
    {
        ARDUCAM_DEBUG("Release image");
        arducam_context.is_image_locked = false;
        --arducam_context.buffer.local_count;
        sli_increment_buffer_pointer(&arducam_context.buffer.tail);
        status = SL_STATUS_OK;
        
    }
    else 
    {
        status = SL_STATUS_EMPTY;
    }

    CORE_EXIT_CRITICAL();

    if(status == SL_STATUS_OK)
    {
        slx_ml_arducam_poll();
    }

    return status;
}

 /***************************************************************************//**
 *  Return number bytes per image
 ******************************************************************************/
uint32_t slx_ml_arducam_calculate_image_buffer_length(arducam_data_format_t format, uint32_t width, uint32_t height)
{
    uint8_t bytes_per_pixel;

    // This camera only returns RGB565 images or YUV422 (i.e. 2 bytes per pixel) 
    switch(format)
    {
    // For RGB88, we expand from RGB565 to RGB88 in-place
    case ARDUCAM_DATA_FORMAT_RGB888:
        bytes_per_pixel = 3;
        break;

    case ARDUCAM_DATA_FORMAT_RGB565:
    case ARDUCAM_DATA_FORMAT_GRAYSCALE:
    case ARDUCAM_DATA_FORMAT_YUV422:
        bytes_per_pixel = 2;
        break;
    default:
        bytes_per_pixel = 0;
        break;
    }

    return bytes_per_pixel * width * height;
}

 /***************************************************************************//**
 *  Return size of image in pixels
 ******************************************************************************/
uint32_t slx_ml_arducam_calculate_image_size(arducam_data_format_t format, uint32_t width, uint32_t height)
{
    uint8_t bytes_per_pixel;

    switch(format)
    {
    case ARDUCAM_DATA_FORMAT_RGB565:
    case ARDUCAM_DATA_FORMAT_YUV422:
        bytes_per_pixel = 2;
        break;

    case ARDUCAM_DATA_FORMAT_RGB888:
        bytes_per_pixel = 3;
        break;
   
    case ARDUCAM_DATA_FORMAT_GRAYSCALE:
        bytes_per_pixel = 1;
        break;

    default:
        bytes_per_pixel = 0;
        break;
    }

    return bytes_per_pixel * width * height;
}

 /***************************************************************************//**
 *  Local Functions
 ******************************************************************************/

 /***************************************************************************//**
 *  This function increments buffer pointer by image size in bytes.
 ******************************************************************************/
void sli_increment_buffer_pointer(uint8_t** ptr)
{
    uint8_t* p = *ptr;

    p += arducam_context.buffer.buffer_bytes_per_image;
    if(p >= arducam_context.buffer.end)
    {
        p = arducam_context.buffer.start;
    }

    *ptr = p;
}

 /***************************************************************************//**
 *  Function to convert image color space.
 ******************************************************************************/
void sli_convert_image_color_space(uint8_t* img)
{
    // Convert for RGB565 to grayscale
    if(arducam_context.data_format == ARDUCAM_DATA_FORMAT_GRAYSCALE)
    {
        const uint8_t *src = img;
        uint8_t *dst = img;

        for(uint32_t pixel_count = arducam_context.buffer.read_length / 2; 
                     pixel_count > 0; --pixel_count)
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
    // Convert for RGB565 to RGB888
    else if(arducam_context.data_format == ARDUCAM_DATA_FORMAT_RGB888)
    {
        const uint8_t *src = img + arducam_context.buffer.read_buffer_offset;
        uint8_t *dst = img;

        for(uint32_t pixel_count = arducam_context.buffer.read_length / 2; pixel_count > 0; --pixel_count)
        {
            uint16_t lb = src[0];
            uint16_t hb = src[1];
            uint16_t rgb565 = lb + (hb<<8);
            dst[0] = (((rgb565 & 0xF800) >> 11) << 3);
            dst[1] = (((rgb565 & 0x7E0) >> 5) << 2);
            dst[2] = (((rgb565 & 0x1F)) << 3);

            src += 2;
            dst += 3;
        }

        if(dst > img + arducam_context.image_size_bytes)
        {
            ARDUCAM_DEBUG("overflow!");
        }
    }
}
