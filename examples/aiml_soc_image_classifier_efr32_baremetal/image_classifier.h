/***************************************************************************//**
 * @file
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
#ifndef IMAGE_CLASSIFIER_H
#define IMAGE_CLASSIFIER_H

#ifdef __cplusplus
extern "C" {
#endif

extern int category_count;

/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/
void image_classifier_init();

const char *get_category_label(int index);
void image_classifier_task();

#ifdef __cplusplus
}
#endif

#endif // IMAGE_CLASSIFIER_H
