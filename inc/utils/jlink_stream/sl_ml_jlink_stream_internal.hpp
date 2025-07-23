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



void slx_ml_jlink_stream_internal_init(uint32_t *trigger_address_ptr, uint32_t *trigger_value_ptr, uint32_t context_address);
void slx_ml_jlink_stream_set_interrupt_enabled(bool enabled);


