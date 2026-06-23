/***************************************************************************//**
 * @file
 * @brief SPI expansion header pin definitions for ArduCam
 *******************************************************************************
 * # License
 * <b>Copyright 2019 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

#ifndef SL_SPIDRV_EXP_CONFIG_H
#define SL_SPIDRV_EXP_CONFIG_H

#include "sl_gpio.h"

// <<< sl:start pin_tool >>>
// <usart signal=TX,RX,CLK,(CS)> SL_SPIDRV_EXP
// $[USART_SL_SPIDRV_EXP]
#ifndef SL_SPIDRV_EXP_PERIPHERAL                
#define SL_SPIDRV_EXP_PERIPHERAL                 USART0
#endif
#ifndef SL_SPIDRV_EXP_PERIPHERAL_NO             
#define SL_SPIDRV_EXP_PERIPHERAL_NO              0
#endif

// USART0 TX on PC13
#ifndef SL_SPIDRV_EXP_TX_PORT                   
#define SL_SPIDRV_EXP_TX_PORT                    SL_GPIO_PORT_C
#endif
#ifndef SL_SPIDRV_EXP_TX_PIN                    
#define SL_SPIDRV_EXP_TX_PIN                     13
#endif

// USART0 RX on PC12
#ifndef SL_SPIDRV_EXP_RX_PORT                   
#define SL_SPIDRV_EXP_RX_PORT                    SL_GPIO_PORT_C
#endif
#ifndef SL_SPIDRV_EXP_RX_PIN                    
#define SL_SPIDRV_EXP_RX_PIN                     12
#endif

// USART0 CLK on PC11
#ifndef SL_SPIDRV_EXP_CLK_PORT                  
#define SL_SPIDRV_EXP_CLK_PORT                   SL_GPIO_PORT_C
#endif
#ifndef SL_SPIDRV_EXP_CLK_PIN                   
#define SL_SPIDRV_EXP_CLK_PIN                    11
#endif

// USART0 CS on PC10
#ifndef SL_SPIDRV_EXP_CS_PORT                   
#define SL_SPIDRV_EXP_CS_PORT                    SL_GPIO_PORT_C
#endif
#ifndef SL_SPIDRV_EXP_CS_PIN                    
#define SL_SPIDRV_EXP_CS_PIN                     10
#endif
// [USART_SL_SPIDRV_EXP]$
// <<< sl:end pin_tool >>>

#endif // SL_SPIDRV_EXP_CONFIG_H
