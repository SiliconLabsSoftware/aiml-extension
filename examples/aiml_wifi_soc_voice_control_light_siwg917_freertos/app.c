/***************************************************************************/ /**
 * @file app.c
 * @brief Wi-Fi AP + Voice Control Light application (FreeRTOS)
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
 #include <stdint.h>
 #include <string.h>
 #include "sl_si91x_driver.h"
 #include "sl_net_wifi_types.h"
 #include "sl_si91x_socket_utility.h"
 #include "sl_si91x_socket_constants.h"
 #include "sl_wifi_callback_framework.h"
 #include "app.h"
 #include "app_ml_bridge.h"
 #include "sl_si91x_core_utilities.h"
 #include "ml/platform/ml_clock_helper.h"
 #include "sl_sleeptimer.h"
 #include "FreeRTOS.h"
 #include "task.h"
 #include "queue.h"
 #ifdef SLI_SI91X_MCU_INTERFACE
 #include "rsi_rom_clks.h"
 #endif

 /******************************************************
  *                      Macros
  ******************************************************/
 /* UDP notify target (used by voice_control_light.cc -> send_message_to_udp_server).
  * Si91x BSD sendto() copies sin_port into the NWP send-request unchanged (confirmed in
  * sl_si91x_bsd_socket.c — no htons/ntohs anywhere in the path). Store the port as a plain
  * host-endian literal; do NOT wrap with htons(). */
 #define SERVER_IP   "192.168.10.11"
 #define SERVER_PORT 5000

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
  *                    Constants
  ******************************************************/
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
  .priority   = osPriorityNormal,
  .tz_module  = 0,
 };

 static const osThreadAttr_t voice_thread_attributes = {
  .name       = "voice",
  .attr_bits  = 0,
  .cb_mem     = 0,
  .cb_size    = 0,
  .stack_mem  = 0,
  /* CMSIS-RTOS2: stack_size is bytes. ML inference + C++ needs more than 4 KiB. */
  .stack_size = 12288,
  .priority   = osPriorityNormal,
  .tz_module  = 0,
 };

 /* Set by voice_task as soon as it runs; read by application_start to confirm thread runs. */
 volatile int g_voice_task_started = 0;

 /* sl_main may call app_init more than once in some builds; a second thread would re-run sl_net_init
  * while the voice task still uses Wi-Fi and breaks UDP. */
 static volatile int s_app_thread_created = 0;
 static int s_udp_first_send_logged        = 0;

 /* Filled in first line of each task to avoid a race on ids returned from osThreadNew. */
 static osThreadId_t s_app_thread_id  = NULL;
 static osThreadId_t s_voice_thread_id = NULL;

 /* Task handles for xTaskNotifyGive / ulTaskNotifyTake (same as CMSIS thread id on this port). */
 static TaskHandle_t s_app_task_freertos   = NULL;
 static TaskHandle_t s_voice_task_freertos = NULL;

 #if (configUSE_TASK_NOTIFICATIONS != 1)
 static volatile uint8_t s_voice_release_from_app = 0;
 #endif

 #if (configUSE_TASK_NOTIFICATIONS == 1)
 /**
  * Wi-Fi v2 callbacks may run from ISR or task context; use vTaskNotifyGiveFromISR vs xTaskNotifyGive
  * per FreeRTOS task.h (same pattern as xTaskNotifyFromISR for other notify actions).
  */
 static void app_notify_app_task_from_wifi_callback(void)
 {
  if (s_app_task_freertos == NULL) {
    return;
  }
  if (xPortIsInsideInterrupt() != pdFALSE) {
    BaseType_t hpw = pdFALSE;
    vTaskNotifyGiveFromISR(s_app_task_freertos, &hpw);
    portYIELD_FROM_ISR(hpw);
  } else {
    (void)xTaskNotifyGive(s_app_task_freertos);
  }
 }
 #endif

 #if (configUSE_MALLOC_FAILED_HOOK == 1)
 /* Incremented from vApplicationMallocFailedHook if pvPortMalloc fails. */
 volatile uint32_t g_freertos_malloc_fail_count = 0;
 #endif

 /** Log FreeRTOS heap (heap_4 / ucHeap). Safe from task context. */
 static void app_log_frt_heap(const char *tag)
 {
  size_t free_now = xPortGetFreeHeapSize();
  size_t min_ever = xPortGetMinimumEverFreeHeapSize();

  printf("[heap:%s] free=%u min_ever=%u total=%u B\n",
         tag,
         (unsigned)free_now,
         (unsigned)min_ever,
         (unsigned)configTOTAL_HEAP_SIZE);
}

#if (configUSE_MALLOC_FAILED_HOOK == 1)
 void vApplicationMallocFailedHook(void)
 {
  g_freertos_malloc_fail_count++;
  printf("\n*** FREERTOS pvPortMalloc FAILED *** count=%lu\n",
         (unsigned long)g_freertos_malloc_fail_count);
  taskDISABLE_INTERRUPTS();
  for (;;) {
    /* Stop here under debugger, or power-cycle. */
  }
}
#endif /* configUSE_MALLOC_FAILED_HOOK */

 static void voice_task(void *argument)
 {
  UNUSED_PARAMETER(argument);
  s_voice_thread_id = osThreadGetId();
  s_voice_task_freertos = xTaskGetCurrentTaskHandle();
  /* Prove thread runs even if printf from this thread never shows on VCOM */
  g_voice_task_started = 1;
#if (configUSE_TASK_NOTIFICATIONS == 1)
  (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
#else
  while (s_voice_release_from_app == 0U) {
    osDelay(10);
  }
#endif
  osDelay(500);
  printf("voice_task: entered\n");
  app_log_frt_heap("voice_task start");

  voice_control_light_task();
  printf("voice_task: inference loop exited\n");
  while (1) {
    osDelay(1000);
  }
 }

 static const sl_wifi_device_configuration_t throughput_configuration = {
   .boot_option = LOAD_NWP_FW,
   .mac_address = NULL,
   .band        = SL_SI91X_WIFI_BAND_2_4GHZ,
   .region_code = US,
   .boot_config = { .oper_mode = SL_SI91X_ACCESS_POINT_MODE,
                    .coex_mode = SL_SI91X_WLAN_ONLY_MODE,
                    .feature_bit_map =
                      (SL_WIFI_FEAT_SECURITY_OPEN | SL_WIFI_FEAT_AGGREGATION | SL_WIFI_FEAT_WPS_DISABLE),
                   .tcp_ip_feature_bit_map = SL_SI91X_TCP_IP_FEAT_DHCPV4_SERVER,
                   .custom_feature_bit_map =
                     (SL_WIFI_SYSTEM_CUSTOM_FEAT_EXTENSION_VALID | SL_SI91X_CUSTOM_FEAT_SOC_CLK_CONFIG_160MHZ),
                   .ext_custom_feature_bit_map = (MEMORY_CONFIG
#ifdef SLI_SI917
                                                  | SL_SI91X_EXT_FEAT_FRONT_END_SWITCH_PINS_ULP_GPIO_4_5_0
#endif
                                                  ),
                   .bt_feature_bit_map         = 0,
                   .ext_tcp_ip_feature_bit_map = 0,
                   .ble_feature_bit_map        = 0,
                   .ble_ext_feature_bit_map    = 0,
                   .config_feature_bit_map     = 0 },
   .ta_pool = { .tx_ratio_in_buffer_pool     = TX_POOL_RATIO,
                .rx_ratio_in_buffer_pool     = RX_POOL_RATIO,
                .global_ratio_in_buffer_pool = GLOBAL_POOL_RATIO }
 };

static volatile bool ap_client_connected = false;
 sl_ip_address_t ip_address          = { 0 };
 sl_net_wifi_ap_profile_t ap_profile = { 0 };

 /* One UDP socket for notify path: repeated socket()+close() on each keyword can stress the NWP
  * (especially with SL_SI91X_EXT_TCP_IP_WAIT_FOR_SOCKET_CLOSE) and correlate with assoc drops. */
 static int s_udp_notify_sock                       = -1;
 static struct sockaddr_in s_udp_notify_peer         = { 0 };
 static bool s_udp_notify_peer_inited                = false;
 static volatile bool s_udp_disconnect_pending       = false;

 /* Staged UDP notify from voice → drained by app thread (see app_udp_notify_*). */
 #define APP_UDP_PENDING_BUF_BYTES   160
 #define APP_UDP_NOTIFY_QUEUE_DEPTH  8

 typedef struct {
  char     data[APP_UDP_PENDING_BUF_BYTES];
  uint32_t len;
 } app_udp_notify_msg_t;

 static QueueHandle_t s_udp_notify_queue = NULL;
 static uint32_t    s_udp_notify_drop_count = 0;

 static void udp_notify_close(void)
 {
  if (s_udp_notify_sock >= 0) {
    close(s_udp_notify_sock);
    s_udp_notify_sock = -1;
  }
  s_udp_notify_peer_inited = false;
  s_udp_first_send_logged = 0;
  s_udp_disconnect_pending = false;
  if (s_udp_notify_queue != NULL) {
    (void)xQueueReset(s_udp_notify_queue);
  }
 }

 /******************************************************
  *               Function Declarations
  ******************************************************/
 static sl_status_t ap_connected_event_handler(sl_wifi_event_t event,
                                               sl_status_t status_code,
                                               void *data,
                                               uint32_t data_length,
                                               void *arg);
 static sl_status_t ap_disconnected_event_handler(sl_wifi_event_t event,
                                                  sl_status_t status_code,
                                                  void *data,
                                                  uint32_t data_length,
                                                  void *arg);
 static void application_start(void *argument);

osThreadId_t app_wifi_thread_id(void)
{
  return s_app_thread_id;
}

osThreadId_t app_voice_thread_id(void)
{
  return s_voice_thread_id;
}

void *app_freertos_app_task_handle_ptr(void)
{
  return (void *)s_app_task_freertos;
}

void *app_freertos_voice_task_handle_ptr(void)
{
  return (void *)s_voice_task_freertos;
}

void app_udp_notify_stage(const char *msg, size_t len)
{
  app_udp_notify_msg_t m;

  if (msg == NULL || len == 0 || s_udp_notify_queue == NULL) {
    return;
  }
  if (len >= APP_UDP_PENDING_BUF_BYTES) {
    len = APP_UDP_PENDING_BUF_BYTES - 1U;
  }
  memcpy(m.data, msg, len);
  m.data[len] = '\0';
  m.len = (uint32_t)len;

  if (xQueueSend(s_udp_notify_queue, &m, 0) != pdPASS) {
    s_udp_notify_drop_count++;
    if (s_udp_notify_drop_count <= 3U || (s_udp_notify_drop_count % 32U) == 0U) {
      printf("WARNING: UDP notify queue full (depth=%d), drop count=%lu\n",
             APP_UDP_NOTIFY_QUEUE_DEPTH,
             (unsigned long)s_udp_notify_drop_count);
    }
  } else {
#if (configUSE_TASK_NOTIFICATIONS == 1)
    if (s_app_task_freertos != NULL) {
      (void)xTaskNotifyGive(s_app_task_freertos);
    }
#endif
  }
}

bool app_udp_notify_is_pending(void)
{
  if (s_udp_notify_queue == NULL) {
    return false;
  }
  return (uxQueueMessagesWaiting(s_udp_notify_queue) > 0U);
}

bool app_udp_notify_consume_and_send(void)
{
  app_udp_notify_msg_t m;
  bool any = false;

  if (s_udp_notify_queue == NULL) {
    return false;
  }
  while (xQueueReceive(s_udp_notify_queue, &m, 0) == pdPASS) {
    any = true;
    send_message_to_udp_server(m.data, (size_t)m.len);
  }
  return any;
}

/******************************************************
 *               Function Definitions
 ******************************************************/
 void app_init(void)
 {
  if (s_app_thread_created) {
    printf("app_init: skip (application thread already started)\n");
    return;
  }
  s_app_thread_created = 1;

  if (s_udp_notify_queue == NULL) {
    s_udp_notify_queue = xQueueCreate(APP_UDP_NOTIFY_QUEUE_DEPTH, sizeof(app_udp_notify_msg_t));
    if (s_udp_notify_queue == NULL) {
      printf("app_init: ERROR xQueueCreate(udp notify) failed (heap?)\n");
      app_log_frt_heap("udp_notify_queue create failed");
      s_app_thread_created = 0;
      return;
    }
  }

  osThreadId_t app_tid = osThreadNew((osThreadFunc_t)application_start, NULL, &thread_attributes);
  osThreadId_t voice_tid = osThreadNew((osThreadFunc_t)voice_task, NULL, &voice_thread_attributes);
  printf("thread id = %p\n", app_tid);
  if (voice_tid == NULL) {
    printf("app_init: ERROR osThreadNew(voice_task) failed (out of heap?)\n");
    app_log_frt_heap("voice_thread create failed");
  }
 }

 static void application_start(void *argument)
 {
  UNUSED_PARAMETER(argument);
  s_app_thread_id = osThreadGetId();
  s_app_task_freertos = xTaskGetCurrentTaskHandle();
  printf("application_start: entered\n");
  app_log_frt_heap("app_start");

  sl_status_t status;
  sl_wifi_firmware_version_t firmware_version = { 0 };

  printf("application_start: before sl_net_init\n");
  status = sl_net_init(SL_NET_WIFI_AP_INTERFACE, &throughput_configuration, NULL, NULL);
  printf("application_start: after sl_net_init status=0x%lx\n", status);

  if (status != SL_STATUS_OK) {
    printf("Wi-Fi AP initialization failed: 0x%lx\n", status);
    return;
  }

  printf("application_start: before callback registration\n");
  sl_wifi_set_callback_v2(SL_WIFI_CLIENT_CONNECTED_EVENTS, ap_connected_event_handler, NULL);
  sl_wifi_set_callback_v2(SL_WIFI_CLIENT_DISCONNECTED_EVENTS, ap_disconnected_event_handler, NULL);
  printf("application_start: after callback registration\n");

  printf("application_start: before sl_wifi_get_firmware_version\n");
  status = sl_wifi_get_firmware_version(&firmware_version);
  printf("application_start: after sl_wifi_get_firmware_version status=0x%lx\n", status);

  if (status != SL_STATUS_OK) {
    return;
  }

  printf("application_start: before sl_net_get_profile\n");
  status = sl_net_get_profile(SL_NET_WIFI_AP_INTERFACE, SL_NET_DEFAULT_WIFI_AP_PROFILE_ID, &ap_profile);
  printf("application_start: after sl_net_get_profile status=0x%lx\n", status);

  if (status != SL_STATUS_OK) {
    return;
  }

  printf("application_start: before sl_net_up\n");
  status = sl_net_up(SL_NET_WIFI_AP_INTERFACE, SL_NET_DEFAULT_WIFI_AP_PROFILE_ID);
  printf("application_start: after sl_net_up status=0x%lx\n", status);
  app_log_frt_heap("after sl_net_up");

  if (status != SL_STATUS_OK) {
    return;
  }

  ip_address.type = SL_IPV4;
  memcpy(&ip_address.ip.v4.bytes, &ap_profile.ip.ip.v4.ip_address.bytes, sizeof(sl_ipv4_address_t));
  printf("AP IP: ");
  print_sl_ip_address(&ip_address);
  printf(" — target %s:%d\n", SERVER_IP, (int)SERVER_PORT);

  printf("application_start: entering wait loop\n");
#if (configUSE_TASK_NOTIFICATIONS == 1)
  while (ulTaskNotifyTake(pdFALSE, 0) > 0U) {
  }
  while (ap_client_connected != true) {
    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  }
#else
  while (ap_client_connected != true) {
    printf("waiting for client\n");
    osDelay(1000);
  }
#endif
  printf("Client connected, waiting 3s for DHCP to complete\n");
  osDelay(3000);

  printf("application_start: before ML / mic init\n");
  ml_configure_clocks_to_max_rate();
  app_led_init();
  app_enable_mic();
  printf("application_start: after ML / mic init\n");

  if (s_udp_notify_queue == NULL) {
    printf("application_start: ERROR udp notify queue missing\n");
    return;
  }

  /* voice_task must set s_voice_thread_id before yield_to_ml will resume it */
  {
    const uint32_t voice_id_timeout_ms = 5000U;
    uint32_t waited_ms = 0U;
    while (s_voice_thread_id == NULL && waited_ms < voice_id_timeout_ms) {
      osDelay(10);
      waited_ms += 10U;
    }
    if (s_voice_thread_id == NULL) {
      printf("application_start: ERROR voice thread id not set after %lu ms\n",
             (unsigned long)voice_id_timeout_ms);
      return;
    }
  }
  printf("application_start: g_voice_task_started = %d (1=voice thread ran)\n", g_voice_task_started);
  app_log_frt_heap("after voice thread ready");
#if (configUSE_MALLOC_FAILED_HOOK == 1)
  printf("application_start: malloc_fail_count=%lu (nonzero = pvPortMalloc failed)\n",
         (unsigned long)g_freertos_malloc_fail_count);
#endif
#if (configUSE_TASK_NOTIFICATIONS == 1)
  (void)xTaskNotifyGive(s_voice_task_freertos);
#else
  s_voice_release_from_app = 1U;
#endif
  /* Cooperate with voice: send staged UDP here, then yield CPU to ML until voice yields back. */
  printf("application_start: entering app <-> voice handoff loop (notify + suspend per config)\n");
#if (configUSE_TASK_NOTIFICATIONS == 1)
  while (ulTaskNotifyTake(pdFALSE, 0) > 0U) {
  }
  for (;;) {
    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (app_udp_notify_consume_and_send()) {
      if (s_voice_task_freertos != NULL) {
        (void)xTaskNotifyGive(s_voice_task_freertos);
      }
    }
    if (s_udp_disconnect_pending) {
      udp_notify_close();
    }
  }
#else
  for (;;) {
    (void)app_udp_notify_consume_and_send();
    if (s_udp_disconnect_pending) {
      udp_notify_close();
    }
    if (s_app_task_freertos != NULL && s_voice_task_freertos != NULL) {
      vTaskResume(s_voice_task_freertos);
      vTaskSuspend(NULL);
    }
  }
#endif
 }

 void app_process_action(void)
 {
 }

void send_message_to_udp_server(const char *msg, size_t len)
{
  socklen_t socket_length = sizeof(struct sockaddr_in);

  if (msg == NULL || len == 0) {
    return;
  }

  if (s_udp_disconnect_pending) {
    udp_notify_close();
  }

  if (!ap_client_connected) {
    return;
  }

  if (!s_udp_notify_peer_inited) {
    memset(&s_udp_notify_peer, 0, sizeof(s_udp_notify_peer));
    s_udp_notify_peer.sin_family = AF_INET;
    s_udp_notify_peer.sin_port   = SERVER_PORT;
    sl_status_t inet_st = sl_net_inet_addr(SERVER_IP, &s_udp_notify_peer.sin_addr.s_addr);
    if (inet_st != SL_STATUS_OK) {
      printf("\nUDP: invalid SERVER_IP \"%s\" (sl_net_inet_addr=0x%lx)\n", SERVER_IP, (unsigned long)inet_st);
      return;
    }
    s_udp_notify_peer_inited = true;
  }

  if (s_udp_notify_sock < 0) {
    s_udp_notify_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_udp_notify_sock < 0) {
      printf("\nSocket creation failed: errno=%d\n", errno);
      return;
    }
    int cr = connect(s_udp_notify_sock,
                     (struct sockaddr *)&s_udp_notify_peer,
                     (socklen_t)sizeof(s_udp_notify_peer));
    if (cr < 0) {
      sl_status_t fw_st = sl_wifi_get_saved_firmware_status();
      printf("\nUDP: connect() failed errno=%d fw=0x%lx\n", errno, (unsigned long)fw_st);
      udp_notify_close();
      return;
    }
  }

  ssize_t sent =
    sendto(s_udp_notify_sock, msg, len, 0, (struct sockaddr *)&s_udp_notify_peer, socket_length);
  if (sent < 0) {
    sl_status_t fw_st = sl_wifi_get_saved_firmware_status();
    printf("\nUDP send failed: sendto=%d errno=%d fw_status=0x%lx (target %s:%d)\n",
           (int)sent,
           errno,
           (unsigned long)fw_st,
           SERVER_IP,
           (int)SERVER_PORT);
    udp_notify_close();
  } else if (s_udp_first_send_logged == 0) {
    s_udp_first_send_logged = 1;
    printf("UDP: first send OK, %d bytes to %s:%d\n", (int)sent, SERVER_IP, (int)SERVER_PORT);
  }
}

 static sl_status_t ap_connected_event_handler(sl_wifi_event_t event,
                                               sl_status_t status_code,
                                               void *data,
                                               uint32_t data_length,
                                               void *arg)
 {
  UNUSED_PARAMETER(data_length);
  UNUSED_PARAMETER(arg);

  if (SL_WIFI_CHECK_IF_EVENT_FAILED(event)) {
    return status_code;
  }

  printf("Remote Client connected: status=0x%lx ", (unsigned long)status_code);
  print_mac_address((sl_mac_address_t *)data);
  printf("\n");

  ap_client_connected = true;
#if (configUSE_TASK_NOTIFICATIONS == 1)
  app_notify_app_task_from_wifi_callback();
#endif
  return SL_STATUS_OK;
 }

 static sl_status_t ap_disconnected_event_handler(sl_wifi_event_t event,
                                                  sl_status_t status_code,
                                                  void *data,
                                                  uint32_t data_length,
                                                  void *arg)
 {
  UNUSED_PARAMETER(data_length);
  UNUSED_PARAMETER(arg);

  if (SL_WIFI_CHECK_IF_EVENT_FAILED(event)) {
    return status_code;
  }

  printf("Remote Client disconnected: status=0x%lx ", (unsigned long)status_code);
  print_mac_address((sl_mac_address_t *)data);
  printf("\n");

  ap_client_connected = false;
  s_udp_disconnect_pending = true;
#if (configUSE_TASK_NOTIFICATIONS == 1)
  app_notify_app_task_from_wifi_callback();
#endif
  return SL_STATUS_OK;
 }

