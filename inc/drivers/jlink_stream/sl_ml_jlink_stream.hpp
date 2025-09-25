/***************************************************************************//**
 * @file
 * @brief Silicon Labs Jlink stream driver.
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
#include <stdbool.h>
#include "sl_status.h"

namespace slx_ml_jlink_stream
{
typedef void (StreamDataCallback)(const char *name, uint32_t length, void *arg);
typedef void (StreamConnectionCallback)(const char *name, bool connected, void *arg);

enum class StreamDirection : uint8_t
{
    Write,
    Read,
};

constexpr const StreamDirection Write = StreamDirection::Write;
constexpr const StreamDirection Read = StreamDirection::Read;


/***************************************************************************//**
 * @brief
 *  Function initializes jlink stream
 * 
 * @return
 *  @ref sl_status_t SL_STATUS_OK when succeed.
 ******************************************************************************/
sl_status_t slx_ml_initialize(void);

/***************************************************************************//**
 * @brief
 *  Function to register jlink stream name and initialize.
 *
 * @param[in] name
 *  String respresenting stream name.
 * 
 * @param[in] direction
 *  @ref StreamDirection stream direction write or read.
 * 
 * @param[in] data_callback
 *  @ref StreamDataCallback pointer holding optional callback for data.
 *
 * @param[in] connection_callback
 *  @ref StreamConnectionCallback pointer holding optional callback for connection.
 * 
 * @param[in] arg
 *  Optional additional arguments.
 * 
 * @return
 *  @ref sl_status_t SL_STATUS_OK for successful initialization.
 ******************************************************************************/
sl_status_t slx_ml_register_stream(const char *name, StreamDirection direction,
                    StreamDataCallback* data_callback = nullptr,
                    StreamConnectionCallback* connection_callback = nullptr, 
                    void *arg = nullptr);

/***************************************************************************//**
 * @brief
 *  Function to un-register specific jlink stream with name.
 *
 * @param[in] name
 *  String respresenting stream name.
 *
 * @return
 *  @ref sl_status_t SL_STATUS_OK for successful completion.
 ******************************************************************************/
sl_status_t slx_ml_unregister_stream(const char *name);

/***************************************************************************//**
 * @brief
 *  Function to check is jlink is connected with name.
 *
 * @param[in] name
 *  String respresenting stream name.
 * 
 * @param[in] connected_ptr
 *  Pointer holding jlink stream connection status.
 *
 * @return
 *  @ref sl_status_t SL_STATUS_OK for successful completion.
 ******************************************************************************/
sl_status_t slx_ml_is_connected(const char *name, bool *connected_ptr);

/***************************************************************************//**
 * @brief
 *  Function to check available bytes to write on specific jlink stream with name.
 *
 * @param[in] name
 *  String respresenting stream name.
 * 
 * @param[out] bytes_available_ptr
 *  Pointer holding number of bytes availabe in jlink stream.
 *
 * @return
 *  @ref sl_status_t SL_STATUS_OK for successful completion.
 ******************************************************************************/
sl_status_t slx_ml_get_bytes_available(const char *name, uint32_t *bytes_available_ptr);

/***************************************************************************//**
 * @brief
 *  Function to write data to jlink stream by name provides bytes written to stream.
 *
 * @param[in] name
 *  String respresenting stream name.
 * 
 * @param[in] data
 *  Pointer holding data to write in jlink stream.
 * 
 * @param[in] length
 *  Length of data to write.
 * 
 * @param[out] bytes_written_ptr
 *  Pointer holding number of bytes written.
 *
 * @return
 *  @ref sl_status_t SL_STATUS_OK for successful completion.
 ******************************************************************************/
sl_status_t slx_ml_write(const char *name, const void *data, uint32_t length, uint32_t *bytes_written_ptr=nullptr);

/***************************************************************************//**
 * @brief
 *  Function to write all data to jlink stream by name.
 *
 * @param[in] name
 *  String respresenting stream name.
 * 
 * @param[in] data
 *  Pointer holding data to write in jlink stream.
 * 
 * @param[in] length
 *  Length of data to write.
 *
 * @return
 *  @ref sl_status_t SL_STATUS_OK for successful completion.
 ******************************************************************************/
sl_status_t slx_ml_write_all(const char *name, const void *data, uint32_t length);


/***************************************************************************//**
 * @brief
 *  Function to read data from jlink stream by name provides bytes read from stream.
 *
 * @param[in] name
 *  String respresenting stream name.
 * 
 * @param[out] data
 *  Pointer holding data to write in jlink stream.
 * 
 * @param[in] length
 *  Length of data to write.
 * 
 * @param[out] bytes_read_ptr
 *  Pointer holding number of bytes read.
 *
 * @return
 *  @ref sl_status_t SL_STATUS_OK for successful completion.
 ******************************************************************************/
sl_status_t slx_ml_read(const char *name, void *data, uint32_t length, uint32_t *bytes_read_ptr=nullptr);

/***************************************************************************//**
 * @brief
 *  This function is called in the platform's interrupt handler. 
 *  It is triggered by the associated Python script 
 ******************************************************************************/
void slx_ml_process_command(void);

} // namespace jlink_stream
