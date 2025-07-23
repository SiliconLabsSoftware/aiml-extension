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
#include <stdbool.h>



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



bool slx_ml_initialize(void);

bool slx_ml_register_stream(const char *name, StreamDirection direction,
                    StreamDataCallback* data_callback = nullptr,
                    StreamConnectionCallback* connection_callback = nullptr, 
                    void *arg = nullptr);
bool slx_ml_unregister_stream(const char *name);

bool slx_ml_is_connected(const char *name, bool *connected_ptr);
bool slx_ml_get_bytes_available(const char *name, uint32_t *bytes_available_ptr);
bool slx_ml_write(const char *name, const void *data, uint32_t length, uint32_t *bytes_written_ptr=nullptr);
bool slx_ml_write_all(const char *name, const void *data, uint32_t length);
bool slx_ml_read(const char *name, void *data, uint32_t length, uint32_t *bytes_read_ptr=nullptr);
void slx_ml_process_command(void);

} // namespace jlink_stream
