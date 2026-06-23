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
 * This files contains example code to demonstrate the GPIO/LED toggle functionality.
 ============================================================================**/

// Include Files
#include "rsi_ccp_user_config.h"

#include "rsi_debug.h"
#include "rgb_led.h"
#include "sl_si91x_clock_manager.h"
#if __has_include("sl_si91x_rgb_led_instances.h")
#include "sl_si91x_rgb_led_instances.h"
#include "sl_si91x_rgb_led.h"
#define SI91X_HAS_RGB 1
#elif __has_include("sl_si91x_led_instances.h")
#include "sl_si91x_led_instances.h"
#include "sl_si91x_led.h"
#define SI91X_HAS_SIMPLE_LED 1
#else
#error "No supported LED instance headers found"
#endif
#include "sl_sleeptimer.h"

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/
/* RGB LED instances*/
/* Map color names to available instances */
#if defined(SI91X_HAS_RGB)

/* RGB build: keep existing names (led_red/led_green/led_blue normally supplied) */
#ifndef RED
#define RED led_red
#endif
#ifndef GREEN
#define GREEN led_green
#endif
#ifndef BLUE
#define BLUE led_blue
#endif

#elif defined(SI91X_HAS_SIMPLE_LED)

/* Single-LED boards: map all colors to the single LED instance */
#ifndef RED
#define RED led_led0
#endif
#ifndef GREEN
#define GREEN led_led0
#endif
#ifndef BLUE
#define BLUE led_led0
#endif

#endif

/* Generic LED ON/OFF macros that select the appropriate API based on config */
#if defined(SI91X_HAS_RGB)
#define RGB_ON(x)  sl_si91x_rgb_led_on(&(x))
#define RGB_OFF(x) sl_si91x_rgb_led_off(&(x))
#elif defined(SI91X_HAS_SIMPLE_LED)
#define RGB_ON(x)  sl_si91x_led_set((x).pin)
#define RGB_OFF(x) sl_si91x_led_clear((x).pin)
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
  // sl_si91x_rgb_led_on(&RED);
  RGB_ON(GREEN);
  // sl_si91x_rgb_led_on(&BLUE);

  /*Extract intensity values from RGB_COLOUR (colour hex code)*/
  red_intensity = ((RGB_COLOUR >> 16) & 0xFF);
  green_intensity = ((RGB_COLOUR >> 8) & 0xFF);
  blue_intensity = (RGB_COLOUR & 0xFF);

  /*Create timer for waking up the system periodically*/
/*  sl_sleeptimer_start_periodic_timer(&timer,
                                     TICK_DELAY,
                                     on_timeout,
                                     NULL,
                                     0,
                                     SL_SLEEPTIMER_NO_HIGH_PRECISION_HF_CLOCKS_REQUIRED_FLAG);*/
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

  /*pulse drive for red*/

  // int8_t count=0;

  // Ensure RED and BLUE are off to avoid color mixing
  RGB_OFF(RED);
  RGB_OFF(BLUE);

  time_period = cycle_percentage;

  blue_time = 0;
  red_time_on = 0;
  red_time_off = 0;
  /*  while(blue_time<=2*time_period)
   {

    if (red_time_off== time_period) {
      sl_si91x_rgb_led_off(&RED);
      red_time_on= 0;
    }
    else if (red_time_on == time_period) {
        sl_si91x_rgb_led_on(&RED);
        red_time_off = 0;
      }
    red_time_on++;
    red_time_off++;
    blue_time++;
    }*/
  // calculate ON time
  uint32_t off_time_ms = time_period;
  uint32_t on_time_ms = TIME_DELAY_MS - off_time_ms;

  // Ensure LED is off first
  RGB_OFF(GREEN);

  // Turn LED ON for calculated time
  if (on_time_ms > 0) {
    RGB_ON(GREEN);
    sl_sleeptimer_delay_millisecond(on_time_ms);
    RGB_OFF(GREEN);
  }

  // Delay for off time
  if (off_time_ms > 0) {
    sl_sleeptimer_delay_millisecond(off_time_ms);
  }

  // Ensure LED is explicitly off at the end
  RGB_OFF(GREEN);

  // pulse drive for green
  /* if (green_time == green_intensity) {
     sl_si91x_rgb_led_off(&GREEN);
   } else {
     if (green_time > PULSE_PERIOD) {
       sl_si91x_rgb_led_on(&GREEN);
       green_time = -1;
     }
   }

   //pulse drive for blue
   if (blue_time == blue_intensity) {
     sl_si91x_rgb_led_off(&BLUE);
   } else {
     if (blue_time > PULSE_PERIOD) {
       sl_si91x_rgb_led_on(&BLUE);
       blue_time = -1;

   }*/

  green_time++;
}
