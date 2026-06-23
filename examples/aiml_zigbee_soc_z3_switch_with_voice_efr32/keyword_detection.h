/***************************************************************************//**
 * @file keyword_detection.h
 * @brief Top level application functions for Zigbee switch with voice
 *        (ML compiler + TensorFlow Lite Micro + MVP keyword spotting)
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
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
#ifndef KEYWORD_DETECTION_H
#define KEYWORD_DETECTION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int category_count;

/***************************************************************************//**
 * Initialize application (model, audio frontend, keyword recognizer, timers).
 ******************************************************************************/
void keyword_detection_init(void);

/***************************************************************************//**
 * Keyword detection task - poll from the main loop when the inference timer
 * has requested a new frame.
 ******************************************************************************/
void keyword_detection_process_action(void);

/***************************************************************************//**
 * Return the string label for a model class index (from flatbuffer metadata).
 ******************************************************************************/
const char *get_category_label(int index);

// Implemented in app.c (Zigbee application callbacks)
void keyword_detected(uint8_t found_command_index);

#ifdef __cplusplus
}
#endif

#endif // KEYWORD_DETECTION_H
