#include <stdio.h>
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "server/include/server.h"
#include "wifi_manager/include/wifi_manager.h"

static esp_err_t mi_handler_1(httpd_req_t *req) {
    httpd_resp_send(req, "Respuesta de mi_handler_1", HTTPD_RESP_USE_STRLEN);
	httpd_resp_set_hdr(req, "Connection", "close");
    return ESP_OK;
}

static esp_err_t mi_handler_2(httpd_req_t *req) {
    httpd_resp_send(req, "Respuesta de mi_handler_2", HTTPD_RESP_USE_STRLEN);
	httpd_resp_set_hdr(req, "Connection", "close");
    return ESP_OK;
}

http_server_uri_t uris[] = {
	{
		.uri = {
			.uri = "/ruta1",
			.method = HTTP_GET,
			.handler = mi_handler_1,
			.user_ctx = NULL
		}
	},
	{
		.uri = {
			.uri = "/ruta2",
			.method = HTTP_POST,
			.handler = mi_handler_2,
			.user_ctx = NULL
		}
	},
};

size_t uris_length = sizeof(uris) / sizeof(uris[0]);

void app_main(void)
{
    
	esp_err_t ret = nvs_flash_init(); // Initialize NVS
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
    wifi_manager_start();
	http_server_start(uris, uris_length);
}