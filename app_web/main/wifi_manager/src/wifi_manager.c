#include "wifi_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/ip4_addr.h"

#define AP_SSID      "JCAR"        
#define AP_PASS      "12345678"
#define STA_SSID     "Kraken_PLUS"
#define STA_PASS     "Rhl418yga"
#define MAX_RETRY    5

static const char *TAG = "wifi_manager";
static EventGroupHandle_t s_wifi_event_group;
static wifi_event_cb_t user_event_cb = NULL;
static int s_retry_num = 0;
static uint8_t s_max_retry = 0;

static QueueHandle_t wifi_app_queue_handle;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static void event_handler_device(esp_event_base_t event_base, int32_t event_id, void* event_data) 
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI("MAIN", "New device connected: "MACSTR, MAC2STR(event->mac));
    }
}

static void event_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                if (s_retry_num < s_max_retry) {
                    esp_wifi_connect();
                    s_retry_num++;
                    ESP_LOGI(TAG, "Retrying connection...");
                } else {
                    xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                }
                break;
            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t* event =(wifi_event_ap_staconnected_t*) event_data;
                ESP_LOGI(TAG, "Station connected: "MACSTR, MAC2STR(event->mac) );
                break;
            }
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    
    if (user_event_cb) {
        user_event_cb(event_base, event_id, event_data);
    }
}

void configure_ip_ap(void){
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (netif == NULL) {
        ESP_LOGE(TAG, "No se encontró la interfaz AP");
        return;
    }

    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 3, 1);  // Nueva IP del AP
    IP4_ADDR(&ip_info.gw, 192, 168, 3, 1);  // Gateway
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);  // Máscara de subred


    ESP_ERROR_CHECK(esp_netif_dhcps_stop(netif)); 
    ESP_ERROR_CHECK(esp_netif_set_ip_info(netif, &ip_info));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(netif));

    ESP_LOGI(TAG, "Nueva IP del AP configurada: " IPSTR, IP2STR(&ip_info.ip));
}

void wifi_manager_init(const char *ap_ssid, const char *ap_pass, 
                       const char *sta_ssid, const char *sta_pass, uint8_t max_retry)
{
    s_max_retry = max_retry;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    configure_ip_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
        
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));

    wifi_config_t wifi_config_ap = {
        .ap = {
            .ssid_len = strlen(ap_ssid),
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        }
    };

    wifi_config_t wifi_config_sta = { 0 };

    strncpy((char*)wifi_config_ap.ap.ssid, ap_ssid, sizeof(wifi_config_ap.ap.ssid));
    strncpy((char*)wifi_config_ap.ap.password, ap_pass, sizeof(wifi_config_ap.ap.password));
    
    strncpy((char*)wifi_config_sta.sta.ssid, sta_ssid, sizeof(wifi_config_sta.sta.ssid));
    strncpy((char*)wifi_config_sta.sta.password, sta_pass, sizeof(wifi_config_sta.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config_ap));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config_sta));
}


void wifi_manager_start(void)
{
    wifi_manager_init(AP_SSID, AP_PASS, STA_SSID, STA_PASS, MAX_RETRY);
    wifi_manager_set_event_callback(event_handler_device);

    esp_err_t ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ERROR WIFI START: %s", esp_err_to_name(ret));
    } 
    ESP_ERROR_CHECK(ret);
    
    s_wifi_event_group = xEventGroupCreate();
    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group, 
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, 
        pdFALSE, 
        pdFALSE, 
        portMAX_DELAY
    );

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP!");
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect");
    }

    vEventGroupDelete(s_wifi_event_group);
}

void wifi_manager_set_event_callback(wifi_event_cb_t callback)
{
    user_event_cb = callback;
}