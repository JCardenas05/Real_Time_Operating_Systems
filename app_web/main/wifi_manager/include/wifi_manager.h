#pragma once
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_system.h"

typedef enum wifi_app_message{
	WIFI_APP_MSG_START_HTTP_SERVER = 0,
	WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER,
	WIFI_APP_MSG_STA_CONNECTED_GOT_IP,
	WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT,
	WIFI_APP_MSG_LOAD_SAVED_CREDENTIALS,
	WIFI_APP_MSG_STA_DISCONNECTED,
} wifi_app_message_e;

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*wifi_event_cb_t)(esp_event_base_t event_base, int32_t event_id, void* event_data);

void wifi_manager_init(const char *ap_ssid, const char *ap_pass, 
                       const char *sta_ssid, const char *sta_pass, uint8_t max_retry);

void wifi_manager_start(void);
void wifi_manager_set_event_callback(wifi_event_cb_t callback);

#ifdef __cplusplus
}
#endif
