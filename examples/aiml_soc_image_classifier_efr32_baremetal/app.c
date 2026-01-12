/***************************************************************************/ /**
 * @file app.c
 * @brief Top level application functions
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

#include "app.h"

#include "image_classifier.h"

/*******************************************************************************
 * Initialize application.
 *
 ******************************************************************************/
extern void app_init(void) {
    image_classifier_init();
}

/*******************************************************************************
 * App ticking function.
 ******************************************************************************/
extern void app_process_action(void) {
    image_classifier_task();
}
