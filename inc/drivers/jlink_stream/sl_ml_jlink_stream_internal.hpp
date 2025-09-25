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


/***************************************************************************//**
 * @brief
 *  Internal driver function to initializ jlink stream
 *
 * @param[out] trigger_address_ptr
 *  Pointer holding address Software Trigger Interrupt Register (STIR)
 *  within the Nested Vectored Interrupt Controller (NVIC) 
 * 
 * @param[out] trigger_value_ptr
 *  Pointer holding value to Interrupt Request.
 * 
 * @param[in] context_address
 *  Pointer holding context address.
 * 
 ******************************************************************************/
void slx_ml_jlink_stream_internal_init(uint32_t *trigger_address_ptr, uint32_t *trigger_value_ptr, uint32_t context_address);

/***************************************************************************//**
 * @brief
 *  Function to enable interrupt.
 *
 * @param[in] enabled
 *  Boolean value to set interrupt.
 * 
 ******************************************************************************/
void slx_ml_jlink_stream_set_interrupt_enabled(bool enabled);


