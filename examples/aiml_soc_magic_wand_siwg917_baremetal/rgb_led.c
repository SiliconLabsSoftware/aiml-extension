/*******************************************************************************
 * @file  rgb_led.c
 * @brief
 *******************************************************************************
 * # License
 * <b>Copyright 2023 Silicon Laboratories Inc. www.silabs.com</b>
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
/**============================================================================
 * @section Description
 * This files contains example code to demonstrate the GPIO/LED toggle functionality.
 ============================================================================**/

// Include Files
#include "rsi_ccp_user_config.h"

#include "sl_sleeptimer.h"
#include "sl_si91x_rgb_led_instances.h"
//#include "rsi_debug.h"
#include "sl_si91x_rgb_led.h"
#include "sl_si91x_clock_manager.h"

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/
/* RGB LED instances*/
#ifndef RED
#define RED led_red
#endif

#ifndef GREEN
#define GREEN led_green
#endif

#ifndef BLUE
#define BLUE led_blue
#endif

/*Delay for PWM simulation*/
#ifndef TICK_DELAY
#define TICK_DELAY 30
#endif

/*Default RGB color (white)*/
#ifndef RGB_COLOUR
#define RGB_COLOUR 0xFFFFFF
#endif

/*Total delay for each PWM cycle*/
#ifndef PULSE_PERIOD
#define PULSE_PERIOD (TICK_DELAY * 0xFF)
#endif

#define SOC_PLL_CLK  ((uint32_t)(180000000)) // 180MHz default SoC PLL Clock as source to Processor
#define INTF_PLL_CLK ((uint32_t)(180000000)) // 180MHz default Interface PLL Clock as source to all peripherals
/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/
uint32_t red_intensity;
uint32_t blue_intensity;
uint32_t green_intensity;

uint32_t red_time_on;
uint32_t red_time_off;
uint32_t blue_time;
uint32_t green_time;
uint32_t rgb_time;
uint32_t time_period;

sl_sleeptimer_timer_handle_t timer;



/***************************************************************************/ /**
 * Turn on functions
 ******************************************************************************/
void turn_on_green(void)
{
  sl_si91x_rgb_led_on(&GREEN);
}

void turn_on_red(void)
{
  sl_si91x_rgb_led_on(&RED);
}
void turn_on_blue(void)
{
  sl_si91x_rgb_led_on(&BLUE);
}

/***************************************************************************/ /**
 * Turn off functions
 ******************************************************************************/

void turn_off_green(void)
{
  sl_si91x_rgb_led_off(&GREEN);
}

void turn_off_red(void)
{
  sl_si91x_rgb_led_off(&RED);
}
void turn_off_blue(void)
{
  sl_si91x_rgb_led_off(&BLUE);
}
