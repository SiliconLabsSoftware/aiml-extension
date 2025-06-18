/*******************************************************************************
 * @file  rgb_led.c
 * @brief
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
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
 * This files contains example code to demonstrate the GPIO/LED toggle
 functionality.
 ============================================================================**/

// Include Files
#include "rsi_ccp_user_config.h"

#include "rsi_debug.h"
#include "sl_si91x_clock_manager.h"
#include "sl_si91x_rgb_led.h"
#include "sl_si91x_rgb_led_instances.h"
#include "sl_sleeptimer.h"

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

#define SOC_PLL_CLK                                                            \
  ((uint32_t)(180000000)) // 180MHz default SoC PLL Clock as source to Processor
#define INTF_PLL_CLK                                                           \
  ((uint32_t)(180000000)) // 180MHz default Interface PLL Clock as source to all
                          // peripherals
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

/*******************************************************************************
 *********************   LOCAL FUNCTION PROTOTYPES   ***************************
 ******************************************************************************/
static void on_timeout(uint32_t cycle_percentage);
static void default_clock_configuration(void);
/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/
// Function to configure clock on powerup
static void default_clock_configuration(void) {
  // Core Clock runs at 180MHz SOC PLL Clock
  sl_si91x_clock_manager_m4_set_core_clk(M4_SOCPLLCLK, SOC_PLL_CLK);

  // All peripherals' source to be set to Interface PLL Clock
  // and it runs at 180MHz
  sl_si91x_clock_manager_set_pll_freq(INFT_PLL, INTF_PLL_CLK,
                                      PLL_REF_CLK_VAL_XTAL);
}
/***************************************************************************/ /**
                                                                               * Initialize RGB LED example.
                                                                               ******************************************************************************/
void rgb_led_init(void) {
  // default clock configuration by application common for whole system
  default_clock_configuration();

  /*Turn on the RGB LED*/
  sl_si91x_rgb_led_on(&GREEN);

  /*Extract intensity values from RGB_COLOUR (colour hex code)*/
  red_intensity = ((RGB_COLOUR >> 16) & 0xFF);
  green_intensity = ((RGB_COLOUR >> 8) & 0xFF);
  blue_intensity = (RGB_COLOUR & 0xFF);
}

/***************************************************************************/ /**
                                                                               * RGB LED ticking function.
                                                                               ******************************************************************************/
void rgb_led_process_action(uint32_t percentage_cycle) {

  on_timeout(percentage_cycle);
  return;
}

/***************************************************************************/ /**
                                                                               * Sleeptimer timeout callback.
                                                                               ******************************************************************************/
static void on_timeout(uint32_t cycle_percentage) {

  time_period = cycle_percentage;

  blue_time = 0;
  red_time_on = 0;
  red_time_off = 0;

  sl_si91x_rgb_led_off(&GREEN);
  sl_sleeptimer_delay_millisecond(time_period);
  sl_si91x_rgb_led_on(&GREEN);

  green_time++;
}
