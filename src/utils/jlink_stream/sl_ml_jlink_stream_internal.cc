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

#include "em_device.h"
#include "sl_ml_jlink_stream_internal.hpp"




#ifndef JLINK_STREAM_IRQ

// The Python script will trigger this IRQ via JLink
// to transfer data between the Python script and the MCU
#if defined(_SILICON_LABS_32B_SERIES_2)
#define JLINK_STREAM_IRQ SW3_IRQ

#elif defined(_SILICON_LABS_32B_SERIES_1)
#define JLINK_STREAM_IRQ FPUEH_IRQ

#else
#error Platform not supported
#endif
#endif // ifndef JLINK_STREAM_IRQ


#define RAM_END_ADDRESS (RAM_MEM_BASE + RAM_MEM_SIZE)
#define CONTEXT_BASE_ADDRESS (RAM_END_ADDRESS-sizeof(uint32_t))

#define SL_CONCAT(A, B) A ## B
#define _JLINK_STREAM_IRQ_N(N) SL_CONCAT(N, n)
#define JLINK_STREAM_IRQ_N _JLINK_STREAM_IRQ_N(JLINK_STREAM_IRQ)

#define _JLINK_STREAM_IRQ_HANDLER(N) SL_CONCAT(N, Handler)
#define JLINK_STREAM_IRQ_HANDLER _JLINK_STREAM_IRQ_HANDLER(JLINK_STREAM_IRQ)


namespace slx_ml_jlink_stream 
{
    void slx_ml_process_command();
}


/*************************************************************************************************/
void slx_ml_jlink_stream_internal_init(uint32_t *trigger_address_ptr, uint32_t *trigger_value_ptr, uint32_t context_address)
{
    NVIC_ClearPendingIRQ(JLINK_STREAM_IRQ_N);
    NVIC_EnableIRQ(JLINK_STREAM_IRQ_N);

    *trigger_address_ptr = (uint32_t)&NVIC->STIR;
    *trigger_value_ptr   = (uint32_t)JLINK_STREAM_IRQ_N;

    volatile uint32_t *base_address = (volatile uint32_t*)CONTEXT_BASE_ADDRESS;
    *base_address = context_address;
}

/*************************************************************************************************/
void slx_ml_jlink_stream_set_interrupt_enabled(bool enabled)
{
    if(enabled)
    {
        __DSB(); // Ensure all data is synchronized before re-enabling interrupts
        NVIC_EnableIRQ(JLINK_STREAM_IRQ_N);
    }
    else
    {
        NVIC_DisableIRQ(JLINK_STREAM_IRQ_N);
        __DSB(); // Ensure all data is synchronized before continuing with interrupts disabled
    }
}


/*************************************************************************************************
 * This interrupt is triggered from an external script. The script uses the ARM NVIC software interrupt (NVIC->STIR)
 * register to trigger this interrupt.
 */
void JLINK_STREAM_IRQ_HANDLER(void)
{
    NVIC_ClearPendingIRQ(JLINK_STREAM_IRQ_N);
    slx_ml_jlink_stream::slx_ml_process_command();
}

