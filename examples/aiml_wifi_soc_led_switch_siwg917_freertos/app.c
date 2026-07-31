/***************************************************************************/ /**
 * @file
 * @brief Wi-Fi UDP LED Switch Application (FreeRTOS)
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
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

#include "sl_board_configuration.h"
#include "cmsis_os2.h"
#include "sl_wifi.h"
#include "sl_net.h"
#include "socket.h"
#include "sl_si91x_socket_support.h"
#include "sl_utility.h"
#include "errno.h"
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "sl_si91x_driver.h"
#include "sl_net_wifi_types.h"
#include "sl_si91x_socket_utility.h"
#include "sl_si91x_socket_constants.h"
#include "sl_si91x_socket.h"
#ifdef SLI_SI91X_MCU_INTERFACE
#include "rsi_rom_clks.h"
#include "sl_si91x_driver_gpio.h"
#include "sl_si91x_gpio.h"
#include "sl_gpio_board.h"
#endif

/******************************************************
 *                      Macros
 ******************************************************/
#define UDP_BUFFER_SIZE 1470
#define LISTENING_PORT  5000

#ifndef TX_POOL_RATIO
#define TX_POOL_RATIO 1
#endif

#ifndef RX_POOL_RATIO
#define RX_POOL_RATIO 1
#endif

#ifndef GLOBAL_POOL_RATIO
#define GLOBAL_POOL_RATIO 1
#endif

/******************************************************
 *               Socket configuration
 ******************************************************/
#define TOTAL_SOCKETS                   1
#define TOTAL_TCP_SOCKETS               0
#define TOTAL_UDP_SOCKETS               1
#define TCP_TX_ONLY_SOCKETS             0
#define TCP_RX_ONLY_SOCKETS             0
#define UDP_TX_ONLY_SOCKETS             0
#define UDP_RX_ONLY_SOCKETS             1
#define TCP_RX_HIGH_PERFORMANCE_SOCKETS 0
#define TCP_RX_WINDOW_SIZE_CAP          44
#define TCP_RX_WINDOW_DIV_FACTOR        44

/******************************************************
 *               Variable Definitions
 ******************************************************/
static const osThreadAttr_t thread_attributes = {
  .name       = "app",
  .attr_bits  = 0,
  .cb_mem     = 0,
  .cb_size    = 0,
  .stack_mem  = 0,
  .stack_size = 3072,
  .priority   = osPriorityLow,
  .tz_module  = 0,
};

static sl_ip_address_t ip_address            = { 0 };
static sl_net_wifi_client_profile_t profile  = { 0 };

static const sl_wifi_device_configuration_t wifi_client_configuration = {
  .boot_option = LOAD_NWP_FW,
  .mac_address = NULL,
  .band        = SL_SI91X_WIFI_BAND_2_4GHZ,
  .region_code = US,
  .boot_config = { .oper_mode = SL_SI91X_CLIENT_MODE,
                   .coex_mode = SL_SI91X_WLAN_ONLY_MODE,
                   .feature_bit_map =
                     (SL_SI91X_FEAT_SECURITY_OPEN | SL_SI91X_FEAT_AGGREGATION | SL_SI91X_FEAT_WPS_DISABLE),
                   .tcp_ip_feature_bit_map =
                     (SL_SI91X_TCP_IP_FEAT_DHCPV4_CLIENT | SL_SI91X_TCP_IP_FEAT_EXTENSION_VALID),
                   .custom_feature_bit_map =
                     (SL_SI91X_CUSTOM_FEAT_EXTENTION_VALID | SL_SI91X_CUSTOM_FEAT_SOC_CLK_CONFIG_160MHZ),
                   .ext_custom_feature_bit_map = (MEMORY_CONFIG
#if defined(SLI_SI917) || defined(SLI_SI915)
                                                  | SL_SI91X_EXT_FEAT_FRONT_END_SWITCH_PINS_ULP_GPIO_4_5_0
#endif
                                                  ),
                   .bt_feature_bit_map         = 0,
                   .ext_tcp_ip_feature_bit_map = (SL_SI91X_EXT_TCP_IP_WINDOW_DIV | SL_SI91X_CONFIG_FEAT_EXTENTION_VALID
                                                  | SL_SI91X_EXT_TCP_IP_WAIT_FOR_SOCKET_CLOSE),
                   .ble_feature_bit_map     = 0,
                   .ble_ext_feature_bit_map = 0,
                   .config_feature_bit_map  = SL_SI91X_FEAT_SLEEP_GPIO_SEL_BITMAP },
  .ta_pool = { .tx_ratio_in_buffer_pool     = TX_POOL_RATIO,
               .rx_ratio_in_buffer_pool     = RX_POOL_RATIO,
               .global_ratio_in_buffer_pool = GLOBAL_POOL_RATIO }
};

static sl_si91x_socket_config_t socket_config = {
  TOTAL_SOCKETS,
  TOTAL_TCP_SOCKETS,
  TOTAL_UDP_SOCKETS,
  TCP_TX_ONLY_SOCKETS,
  TCP_RX_ONLY_SOCKETS,
  UDP_TX_ONLY_SOCKETS,
  UDP_RX_ONLY_SOCKETS,
  TCP_RX_HIGH_PERFORMANCE_SOCKETS,
  TCP_RX_WINDOW_SIZE_CAP,
  TCP_RX_WINDOW_DIV_FACTOR
};

#ifdef SLI_SI91X_MCU_INTERFACE
static sl_si91x_gpio_pin_config_t app_status_led_gpio = {
  { SL_SI91X_GPIO_15_PORT, 15 },
  GPIO_OUTPUT
};

#define APP_STATUS_LED_ACTIVE_LOW 1

typedef enum {
  APP_LED_ACTION_NONE = 0,
  APP_LED_ACTION_FORCE_ON,
  APP_LED_ACTION_FORCE_OFF,
} app_led_action_t;

static volatile app_led_action_t app_led_pending_action = APP_LED_ACTION_NONE;
#endif

/******************************************************
 *               Function Declarations
 ******************************************************/
static void app_led_init(void);
static void app_led_set(bool level_high);
static void app_apply_socket_payload_led(const uint8_t *data, uint32_t length);
static void app_led_run_feedback_if_pending(void);
static void application_start(void *argument);

/******************************************************
 *               LED Functions
 ******************************************************/
#ifdef SLI_SI91X_MCU_INTERFACE
static void app_led_init(void)
{
  sl_status_t s = sl_gpio_driver_init();
  if (s != SL_STATUS_OK) {
    printf("\r\nGPIO init failed: 0x%lx\r\n", (unsigned long)s);
    return;
  }
  s = sl_gpio_set_configuration(app_status_led_gpio);
  if (s != SL_STATUS_OK) {
    printf("\r\nGPIO LED config failed: 0x%lx\r\n", (unsigned long)s);
    return;
  }
  app_led_set(false);
}

static void app_led_set(bool level_high)
{
  bool drive_high = level_high;
#if APP_STATUS_LED_ACTIVE_LOW
  drive_high = !level_high;
#endif
  if (drive_high) {
    (void)sl_gpio_driver_set_pin(&app_status_led_gpio.port_pin);
  } else {
    (void)sl_gpio_driver_clear_pin(&app_status_led_gpio.port_pin);
  }
}
#else
static void app_led_init(void)
{
}

static void app_led_set(bool level_high)
{
  (void)level_high;
}
#endif

static void app_led_run_feedback_if_pending(void)
{
#ifdef SLI_SI91X_MCU_INTERFACE
  app_led_action_t action = app_led_pending_action;
  if (action == APP_LED_ACTION_NONE) {
    return;
  }
  app_led_pending_action = APP_LED_ACTION_NONE;

  if (action == APP_LED_ACTION_FORCE_OFF) {
    app_led_set(false);
    return;
  }

  if (action == APP_LED_ACTION_FORCE_ON) {
    app_led_set(true);
  }
#endif
}

static void app_apply_socket_payload_led(const uint8_t *data, uint32_t length)
{
  char scratch[256];
  size_t copy_len;

  if (data == NULL || length == 0) {
    return;
  }
  copy_len = length < (sizeof(scratch) - 1) ? length : (sizeof(scratch) - 1);
  memcpy(scratch, data, copy_len);
  scratch[copy_len] = '\0';

  if (strstr(scratch, "off") != NULL) {
#ifdef SLI_SI91X_MCU_INTERFACE
    app_led_set(false);
    app_led_pending_action = APP_LED_ACTION_FORCE_OFF;
#endif
    printf("\r\nLED command: OFF\r\n");
  } else if (strstr(scratch, "on") != NULL) {
#ifdef SLI_SI91X_MCU_INTERFACE
    app_led_set(true);
    app_led_pending_action = APP_LED_ACTION_FORCE_ON;
#endif
    printf("\r\nLED command: ON\r\n");
  }
}

/******************************************************
 *               Async Socket Callback
 ******************************************************/
static uint8_t first_data_frame = 1;

static void data_callback(uint32_t sock_no,
                           uint8_t *buffer,
                           uint32_t length,
                           const sl_si91x_socket_metadata_t *firmware_socket_response)
{
  UNUSED_PARAMETER(firmware_socket_response);

  if (first_data_frame) {
    first_data_frame = 0;
    printf("\r\nSocket ID : %lu\r\n", (unsigned long)sock_no);
    printf("\r\nSTA UDP on port %d — payload substring 'on' / 'off' controls LED\r\n", LISTENING_PORT);
  }
  app_apply_socket_payload_led(buffer, length);
}

/******************************************************
 *               Application Entry
 ******************************************************/
void app_init(void)
{
  osThreadNew((osThreadFunc_t)application_start, NULL, &thread_attributes);
}

void app_process_action(void)
{
}

static void application_start(void *argument)
{
  UNUSED_PARAMETER(argument);
  sl_status_t status;

  app_led_init();
  sl_mac_address_t mac_addr                   = { 0 };
  sl_wifi_firmware_version_t firmware_version = { 0 };

  status = sl_net_init(SL_NET_WIFI_CLIENT_INTERFACE, &wifi_client_configuration, NULL, NULL);
  if (status != SL_STATUS_OK) {
    printf("\r\nFailed to start Wi-Fi Client interface: 0x%lx\r\n", (unsigned long)status);
    return;
  }
  printf("\r\nWi-Fi client interface init success\r\n");

  status = sl_wifi_get_mac_address(SL_WIFI_CLIENT_INTERFACE, &mac_addr);
  if (status != SL_STATUS_OK) {
    printf("\r\nFailed to get mac address: 0x%lx\r\n", (unsigned long)status);
    return;
  }
  printf("\r\nDevice MAC address: %x:%x:%x:%x:%x:%x\r\n",
         mac_addr.octet[0],
         mac_addr.octet[1],
         mac_addr.octet[2],
         mac_addr.octet[3],
         mac_addr.octet[4],
         mac_addr.octet[5]);

  status = sl_wifi_get_firmware_version(&firmware_version);
  if (status != SL_STATUS_OK) {
    printf("\r\nFailed to fetch firmware version: 0x%lx\r\n", (unsigned long)status);
    return;
  } else {
    print_firmware_version(&firmware_version);
  }

  status = sl_net_up(SL_NET_WIFI_CLIENT_INTERFACE, SL_NET_DEFAULT_WIFI_CLIENT_PROFILE_ID);
  if (status != SL_STATUS_OK) {
    printf("\r\nFailed to connect to AP: 0x%lx\r\n", (unsigned long)status);
    return;
  }
  printf("\r\nWi-Fi client connected\r\n");

  status = sl_net_get_profile(SL_NET_WIFI_CLIENT_INTERFACE, SL_NET_DEFAULT_WIFI_CLIENT_PROFILE_ID, &profile);
  if (status != SL_STATUS_OK) {
    printf("Failed to get client profile: 0x%lx\r\n", (unsigned long)status);
    return;
  }
  printf("\r\nSuccess to get client profile\r\n");

  ip_address.type = SL_IPV4;
  memcpy(&ip_address.ip.v4.bytes, &profile.ip.ip.v4.ip_address.bytes, sizeof(sl_ipv4_address_t));
  print_sl_ip_address(&ip_address);

  /* Configure and open a UDP async socket for listening. */
  int client_socket;
  int socket_return_value;
  struct sockaddr_in server_address = { 0 };
  socklen_t socket_length           = sizeof(struct sockaddr_in);

  sl_status_t cfg_status = sl_si91x_config_socket(socket_config);
  if (cfg_status != SL_STATUS_OK) {
    printf("Socket config failed: 0x%lx\r\n", (unsigned long)cfg_status);
  }
  printf("\r\nSocket config Done\r\n");

  client_socket = sl_si91x_socket_async(AF_INET, SOCK_DGRAM, IPPROTO_UDP, &data_callback);
  if (client_socket < 0) {
    printf("\r\nSocket creation failed with bsd error: %d\r\n", errno);
    return;
  }
  printf("\r\nSocket ID : %d\r\n", client_socket);

  server_address.sin_family = AF_INET;
  server_address.sin_port   = LISTENING_PORT;

  socket_return_value = sl_si91x_bind(client_socket, (struct sockaddr *)&server_address, socket_length);
  if (socket_return_value < 0) {
    printf("\r\nSocket bind failed with bsd error: %d\r\n", errno);
    close(client_socket);
    return;
  }
  printf("\r\nListening on Local Port %d\r\n", LISTENING_PORT);

  printf("\r\nSTA UDP: send datagrams whose payload contains substring 'on' or 'off'\r\n");
  while (1) {
    app_led_run_feedback_if_pending();
    osThreadYield();
  }
}
