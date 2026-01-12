/***************************************************************************//**
 * @file sl_tlv.h
 * @brief Define the payload of the messages.
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
#ifndef SL_TLV_H
#define SL_TLV_H
/* Generic TLV entry: [type:1][length:1][value:N] 
The following packets define the payload of the messages. To structure the whole message,
 think of them as (Debug Channel Header + Payload + End Framing).
*/
typedef struct {
    uint8_t type;     /* TLV_* */
    uint8_t length;   /* N: number of bytes in value */
    uint8_t value[];  /* N bytes follow */
} sl_tlv_t;

#endif // SL_TLV_H
