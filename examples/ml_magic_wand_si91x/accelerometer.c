/***************************************************************************//**
 * @file
 * @brief Functionality for reading accelerometer data from IMU
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories Inc. www.silabs.com</b>
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

 #include "accelerometer.h"
 #include "constants.h"
 #include "sl_si91x_imu.h"
 #include "sl_si91x_peripheral_gpio.h"
 #include "sl_si91x_icm40627.h"
 #include "sl_si91x_driver_gpio.h"
 #include "sl_si91x_clock_manager.h"
 #include "sl_si91x_ssi.h"
 #include "rsi_rom_clks.h"
 #include "rsi_debug.h"
 #include "sl_sleeptimer.h"
 #include "stdio.h"
 #include "sl_driver_gpio.h"
 #include <math.h>
 
 #if defined(SL_COMPONENT_CATALOG_PRESENT)
 #include "sl_component_catalog.h"
 #endif
 
 /*******************************************************************************
  ***************************  Defines / Macros  ********************************
  ******************************************************************************/
 
 // Filter constants
 #define ALPHA 0.98f
 #define RAD_TO_DEG 57.2958f
 #define DT 0.04f
 
 // SSI Master configuration
 #define SSI_MASTER_DIVISION_FACTOR         0         // Division Factor
 #define SSI_MASTER_INTF_PLL_CLK            180000000 // PLL Clock frequency
 #define SSI_MASTER_INTF_PLL_REF_CLK        40000000  // PLL Ref Clock frequency
 #define SSI_MASTER_SOC_PLL_CLK             20000000  // SOC PLL Clock frequency
 #define SSI_MASTER_SOC_PLL_REF_CLK         40000000  // SOC PLL REFERENCE CLOCK frequency
 #define SSI_MASTER_INTF_PLL_500_CTRL_VALUE 0xD900    // Interface PLL control value
 #define SSI_MASTER_SOC_PLL_MM_COUNT_LIMIT  0xA4      // SOC PLL count limit
 #define SSI_MASTER_BIT_WIDTH               8         // SSI bit width
 #define SSI_MASTER_BAUDRATE                10000000  // SSI baudrate
 #define SSI_MASTER_RECEIVE_SAMPLE_DELAY    0         // By default sample delay is 0
 #define DELAY_PERIODIC_MS1                 200       // sleeptimer1 periodic timeout in ms
 
 // Clock configuration
 #define SOC_PLL_CLK  ((uint32_t)(180000000)) // 180MHz default SoC PLL Clock as source to Processor
 #define INTF_PLL_CLK ((uint32_t)(180000000)) // 180MHz default Interface PLL Clock as source to all peripherals
 
 // Boolean definitions
 #define true  1
 #define false 0
 
 // IMU buffer configuration
 #define IMU_BUFFER_SIZE   200
 /*******************************************************************************
  ******************************  Data Types  ***********************************
  ******************************************************************************/
 
 // Accelerometer data from sensor
 typedef struct imu_data {
   int16_t x;
   int16_t y;
   int16_t z;
 } imu_data_t;
 
 /*******************************************************************************
  *************************** LOCAL VARIABLES   *******************************
  ******************************************************************************/
 
 // Timer handles
 sl_sleeptimer_timer_handle_t timer1; // sleeptimer1 handle
 boolean_t delay_timeout;             // Indicates sleeptimer1 timeout
 
 // SSI driver variables
 static sl_ssi_handle_t ssi_driver_handle = NULL;
 static boolean_t ssi_master_transfer_complete;
 static uint32_t ssi_slave_number = SSI_SLAVE_0;
 
 // GPIO configuration
 static sl_si91x_gpio_pin_config_t sl_imu_pin_config = { { IMU_interrupt_PORT, IMU_interrupt_PIN }, GPIO_INPUT };
 
 // IMU data buffer
 static imu_data_t buffer[IMU_BUFFER_SIZE];
 static int head_ptr = 0;        // Current head pointer of the buffer
 static bool init_done = false;  // Wait for buffer to be filled first time
 static bool imu_ready = false;  // IMU data ready flag
 
 // Debug counter
 int count = 0;
 
 /*******************************************************************************
  **********************  Local Function prototypes   ***************************
  ******************************************************************************/
 static sl_status_t ssi_master_init_clock_configuration_structure(sl_ssi_clock_config_t *clock_config);
 static void ssi_master_callback_event_handler(uint32_t event);
 void on_data_available();
 void process_imu_data();
 
 /*******************************************************************************
  **************************   GLOBAL FUNCTIONS   *******************************
  ******************************************************************************/
 
 /**
  * @brief Called when the IMU has data available using GPIO interrupt
  */
 void on_data_available()
 {
   imu_ready = true;
 }
 
 /**
  * @brief Process IMU data and store in circular buffer
  */
 void process_imu_data()
 {
   if (imu_ready == false) {
     return;
   }
 
   imu_ready = false;
   sl_imu_update();
 
   int16_t acceleration[3];
   sl_imu_get_acceleration(acceleration);
 
   // Store data in circular buffer
   buffer[head_ptr].x = acceleration[0];
   buffer[head_ptr].y = acceleration[1];
   buffer[head_ptr].z = acceleration[2];
   head_ptr++;
 
   // Check if we have enough data for processing
   if (head_ptr >= SEQUENCE_LENGTH) {
     init_done = true;
   }
 
   // Wrap around buffer
   if (head_ptr >= IMU_BUFFER_SIZE) {
     head_ptr = 0;
     init_done = true;
   }
 }
 
 /**
  * @brief Initialize accelerometer and SSI interface
  * @return SL_STATUS_OK on success, error code on failure
  */
 sl_status_t accelerometer_setup(void)
 {
   sl_status_t sl_status;
   sl_status_t status = SL_STATUS_FAIL;
   sl_ssi_clock_config_t ssi_clock_config;
   sl_ssi_version_t ssi_version;
   uint8_t dev_id;
 
   // Configure SSI master configuration structure
   sl_ssi_control_config_t ssi_master_config;
   ssi_master_config.bit_width            = SSI_MASTER_BIT_WIDTH;
   ssi_master_config.device_mode          = SL_SSI_ULP_MASTER_ACTIVE;
   ssi_master_config.clock_mode           = SL_SSI_PERIPHERAL_CPOL0_CPHA0;
   ssi_master_config.baud_rate            = SSI_MASTER_BAUDRATE;
   ssi_master_config.receive_sample_delay = SSI_MASTER_RECEIVE_SAMPLE_DELAY;
 
 
 #if defined(SENSOR_ENABLE_GPIO_MAPPED_TO_UULP)
   if (sl_si91x_gpio_driver_get_uulp_npss_pin(SENSOR_ENABLE_GPIO_PIN) != 1) {
     // Enable GPIO ULP_CLK
     status = sl_si91x_gpio_driver_enable_clock((sl_si91x_gpio_select_clock_t)ULPCLK_GPIO);
     if (status != SL_STATUS_OK) {
       printf("sl_si91x_gpio_driver_enable_clock, Error code: %lu\n", status);
     }
     printf("GPIO driver clock enable is successful\n");
 
     // Set NPSS GPIO pin MUX
     status = sl_si91x_gpio_driver_set_uulp_npss_pin_mux(SENSOR_ENABLE_GPIO_PIN, NPSS_GPIO_PIN_MUX_MODE0);
     if (status != SL_STATUS_OK) {
       printf("sl_si91x_gpio_driver_set_uulp_npss_pin_mux, Error code: %lu\n", status);
     }
     printf("GPIO driver uulp pin mux selection is successful\n");
 
     // Set NPSS GPIO pin direction
     status = sl_si91x_gpio_driver_set_uulp_npss_direction(SENSOR_ENABLE_GPIO_PIN, (sl_si91x_gpio_direction_t)GPIO_OUTPUT);
     if (status != SL_STATUS_OK) {
       printf("sl_si91x_gpio_driver_set_uulp_npss_direction, Error code: %lu\n", status);
     }
     printf("GPIO driver uulp pin direction selection is successful\n");
 
     // Set UULP GPIO pin
     status = sl_si91x_gpio_driver_set_uulp_npss_pin_value(SENSOR_ENABLE_GPIO_PIN, GPIO_PIN_SET);
     if (status != SL_STATUS_OK) {
       printf("sl_si91x_gpio_driver_set_uulp_npss_pin_value, Error code: %lu\n", status);
     }
     printf("GPIO driver set uulp pin value is successful\n");
   }
 #endif
 
   do {
     // Get version information of SSI driver
     ssi_version = sl_si91x_ssi_get_version();
     printf("SSI version is fetched successfully\n");
     printf("API version is %d.%d.%d\n", ssi_version.release, ssi_version.major, ssi_version.minor);
 
     // Clock configuration for the SSI driver
     sl_status = ssi_master_init_clock_configuration_structure(&ssi_clock_config);
     if (sl_status != SL_STATUS_OK) {
       printf("SSI Clock get Configuration Failed, Error Code: %lu\n", sl_status);
       break;
     }
     printf("SSI Clock get Configuration Success\n");
 
     sl_status = sl_si91x_ssi_configure_clock(&ssi_clock_config);
     if (sl_status != SL_STATUS_OK) {
       printf("SSI Clock Configuration Failed, Error Code: %lu\n", sl_status);
       break;
     }
     printf("SSI Clock Configuration Success\n");
 
     // Initialize the SSI driver
     sl_status = sl_si91x_ssi_init(ssi_master_config.device_mode, &ssi_driver_handle);
     if (sl_status != SL_STATUS_OK) {
       printf("SSI Initialization Failed, Error Code: %lu\n", sl_status);
       break;
     }
     printf("SSI Initialization Success\n");
 
     // Configure the SSI to Master, 8-bit mode @10000 kBits/sec
     sl_status = sl_si91x_ssi_set_configuration(ssi_driver_handle, &ssi_master_config, SSI_SLAVE_0);
     if (sl_status != SL_STATUS_OK) {
       printf("Failed to Set Configuration Parameters to SSI, Error Code: %lu\n", sl_status);
       break;
     }
     printf("Set Configuration Parameters to SSI\n");
 
     // Register the user callback
     sl_status = sl_si91x_ssi_register_event_callback(ssi_driver_handle, ssi_master_callback_event_handler);
     if (sl_status != SL_STATUS_OK) {
       printf("SSI register event callback Failed, Error Code: %lu\n", sl_status);
       break;
     }
     printf("SSI register event callback Success\n");
 
     // Print current clock division factor
     printf("Current Clock division factor is %lu\n", sl_si91x_ssi_get_clock_division_factor(ssi_driver_handle));
 
     // Set the slave number
     sl_si91x_ssi_set_slave_number((uint8_t)ssi_slave_number);
 
     // Reset the sensor
     sl_status = sl_si91x_icm40627_software_reset(ssi_driver_handle);
     if (sl_status != SL_STATUS_OK) {
       printf("Sensor Software reset unsuccessful, Error Code: 0x%ld\n", sl_status);
       break;
     } else {
       printf("Software reset successful\n");
     }
 
     // Read Who am I register, should get ICM40627_DEVICE_ID
     sl_status = sl_si91x_icm40627_get_device_id(ssi_driver_handle, &dev_id);
     if ((sl_status == SL_STATUS_OK) && (dev_id == ICM40627_DEVICE_ID)) {
       printf("Successfully verified ICM40627 Device by ID\n");
     } else {
       printf("ICM40627 Get Device ID failed\n");
     }
 
     // Initialize accelerometer sensor
     status = sl_imu_init(ssi_driver_handle);
     if (status != SL_STATUS_OK) {
       return status;
     }
 
     sl_imu_configure(ACCELEROMETER_FREQ, ssi_driver_handle);
 
     // Configure GPIO for interrupt
     sl_gpio_set_configuration(sl_imu_pin_config);
 
     // Configure GPIO interrupt
     sl_status = sl_si91x_gpio_driver_configure_uulp_interrupt(
         SL_GPIO_INTERRUPT_FALL_EDGE, IMU_interrupt_PIN, on_data_available);
     if (sl_status != SL_STATUS_OK) {
       printf("Interrupt callback setup failed, Error Code: %lu\n", sl_status);
       break;
     }
 
     printf("Sensor and interrupt configuration completed successfully\n");
     status = SL_STATUS_OK;
 
   } while (false);
 
   return status;
 
 }
 
 
 
 
 /**
  * @brief Read accelerometer data from circular buffer
  * @param dst Destination buffer to store accelerometer data
  * @param n Number of samples to read
  * @return SL_STATUS_OK on success, SL_STATUS_FAIL on failure
  */
 sl_status_t accelerometer_read(acc_data_t* dst, int n)
 {
   process_imu_data();
 
   if (!init_done) {
     return SL_STATUS_FAIL;
   }
 
   for (int i = 0; i < n; i++) {
     int index = head_ptr - n + i;
     if (index < 0) {
       index += IMU_BUFFER_SIZE;
     }
     imu_data_t src = buffer[index];
 
     dst[i].x = src.x;
     dst[i].y = src.y;
     dst[i].z = src.z;
   }
 
   return SL_STATUS_OK;
 }
 
 
 /*******************************************************************************
  **************************   LOCAL FUNCTIONS   ********************************
  ******************************************************************************/
 /**
  * @brief SSI Master callback handler
  * @param[in] event SSI Master transmit and receive events
  */
 static void ssi_master_callback_event_handler(uint32_t event)
 {
   switch (event) {
     case SSI_EVENT_TRANSFER_COMPLETE:
       ssi_master_transfer_complete = true;
       break;
 
     case SSI_EVENT_DATA_LOST:
       // Occurs in slave mode when data is requested/sent by master
       // but send/receive/transfer operation has not been started
       // and indicates that data is lost. Occurs also in master mode
       // when driver cannot transfer data fast enough.
       break;
 
     case SSI_EVENT_MODE_FAULT:
       // Occurs in master mode when Slave Select is deactivated and
       // indicates Master Mode Fault.
       break;
   }
 }
 /**
  * @brief Set the values in the SSI Master clock config structure
  * @param[in] clock_config Clock configuration structure
  * @return SL_STATUS_OK if set was fine, SL_STATUS_NULL_POINTER if NULL ptr passed in
  */
 static sl_status_t ssi_master_init_clock_configuration_structure(sl_ssi_clock_config_t *clock_config)
 {
   if (clock_config == NULL) {
     return SL_STATUS_NULL_POINTER;
   }
 
   clock_config->soc_pll_mm_count_value     = SSI_MASTER_SOC_PLL_MM_COUNT_LIMIT;
   clock_config->intf_pll_500_control_value = SSI_MASTER_INTF_PLL_500_CTRL_VALUE;
   clock_config->intf_pll_clock             = SSI_MASTER_INTF_PLL_CLK;
   clock_config->intf_pll_reference_clock   = SSI_MASTER_INTF_PLL_REF_CLK;
   clock_config->soc_pll_clock              = SSI_MASTER_SOC_PLL_CLK;
   clock_config->soc_pll_reference_clock    = SSI_MASTER_SOC_PLL_REF_CLK;
   clock_config->division_factor            = SSI_MASTER_DIVISION_FACTOR;
 
   return SL_STATUS_OK;
 }
 
 