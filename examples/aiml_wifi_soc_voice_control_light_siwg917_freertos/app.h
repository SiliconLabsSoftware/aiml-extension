/***************************************************************************/ /**
 * @file app.h
 * @brief Top level application hooks (called from sl_main).
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 ******************************************************************************/

#ifndef APP_H
#define APP_H

#include "cmsis_os2.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void app_init(void);
void app_process_action(void);
void send_message_to_udp_server(const char *msg, size_t len);

/**
 * Opaque FreeRTOS TaskHandle_t for the application (Wi-Fi) task and the voice (ML) task.
 * Include FreeRTOS.h + task.h (e.g. from C++) and cast to TaskHandle_t for vTaskResume /
 * ulTaskNotifyTake, etc.
 */
void *app_freertos_app_task_handle_ptr(void);
void *app_freertos_voice_task_handle_ptr(void);

/**
 * Voice thread: enqueue a UDP notify payload (FreeRTOS queue, multiple messages supported).
 * If the queue is full, the message is dropped and a rate-limited warning is printed.
 * Voice thread: call app_udp_notify_is_pending() after process_output(); if true, block with
 * FreeRTOS until the app task drains the queue (see voice_control_light.cc). The app thread runs
 * app_udp_notify_consume_and_send() inside application_start (not called from voice).
 */
void app_udp_notify_stage(const char *msg, size_t len);
bool app_udp_notify_is_pending(void);
/** @return true if at least one queued message was sent (voice may be waiting for this). */
bool app_udp_notify_consume_and_send(void);

osThreadId_t app_wifi_thread_id(void);
osThreadId_t app_voice_thread_id(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
