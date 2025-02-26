#include "server.h"

static const char TAG[] = "http_server";

static httpd_handle_t http_server_handle = NULL; // HTTP server task handle
static TaskHandle_t task_http_server_monitor = NULL; // HTTP server monitor task handle
static QueueHandle_t http_server_monitor_queue_handle;

extern const uint8_t index_html_start[]				asm("_binary_index_html_start");
extern const uint8_t index_html_end[]				asm("_binary_index_html_end");
extern const uint8_t lectura_pot_html_start[]		asm("_binary_lectura_pot_html_start");
extern const uint8_t lectura_pot_html_end[]			asm("_binary_lectura_pot_html_end");
extern const uint8_t lectura_temp_html_start[]		asm("_binary_lectura_temp_html_start");
extern const uint8_t lectura_temp_html_end[]		asm("_binary_lectura_temp_html_end");
extern const uint8_t led_rgb_html_start[]			asm("_binary_led_rgb_html_start");
extern const uint8_t led_rgb_html_end[]				asm("_binary_led_rgb_html_end"); 
extern const uint8_t credenciales_html_start[]		asm("_binary_credenciales_html_start");
extern const uint8_t credenciales_html_end[]		asm("_binary_credenciales_html_end");
extern const uint8_t app_css_start[]				asm("_binary_styles_css_start");
extern const uint8_t app_css_end[]					asm("_binary_styles_css_end");
extern const uint8_t app_js_start[]					asm("_binary_scripts_js_start");
extern const uint8_t app_js_end[]					asm("_binary_scripts_js_end");

/**
 * HTTP server monitor task used to track events of the HTTP server
 * @param pvParameters parameter which can be passed to the task.
 */
static void http_server_monitor(void *parameter)
{
	http_server_queue_message_t msg;
	for (;;)
	{
		if (xQueueReceive(http_server_monitor_queue_handle, &msg, portMAX_DELAY))
		{
			switch (msg.msgID)
			{
				case HTTP_MSG_WIFI_CONNECT_INIT:
					ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_INIT");
					break;
				case HTTP_MSG_WIFI_CONNECT_SUCCESS:
					ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_SUCCESS");
					break;
				case HTTP_MSG_WIFI_CONNECT_FAIL:
					ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_FAIL");
					break;
				default:
					break;
			}
		}
	}
}

static esp_err_t http_server_index_html_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "index.html requested");
	httpd_resp_set_type(req, "text/html");
	httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);
	return ESP_OK;
}

static esp_err_t http_server_lectura_pot_html_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "lectura_pot.html requested");
	httpd_resp_set_type(req, "text/html");
	httpd_resp_send(req, (const char *)lectura_pot_html_start, lectura_pot_html_end - lectura_pot_html_start);
	return ESP_OK;
}

static esp_err_t http_server_lectura_temp_html_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "lectura_temp.html requested");
	httpd_resp_set_type(req, "text/html");
	httpd_resp_send(req, (const char *)lectura_temp_html_start, lectura_temp_html_end - lectura_temp_html_start);
	return ESP_OK;
}

static esp_err_t http_server_led_rgb_html_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "led_rgb.html requested");
	httpd_resp_set_type(req, "text/html");
	httpd_resp_send(req, (const char *)led_rgb_html_start, led_rgb_html_end - led_rgb_html_start);
	return ESP_OK;
}

static esp_err_t http_server_credenciales_html_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "credenciales.html requested");
	httpd_resp_set_type(req, "text/html");
	httpd_resp_send(req, (const char *)credenciales_html_start, credenciales_html_end - credenciales_html_start);
	return ESP_OK;
}

static esp_err_t http_server_app_css_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "styles.css requested");
	httpd_resp_set_type(req, "text/css");
	httpd_resp_send(req, (const char *)app_css_start, app_css_end - app_css_start);
	return ESP_OK;
}

static esp_err_t http_server_app_js_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "scripts.js requested");
	httpd_resp_set_type(req, "application/javascript");
	httpd_resp_send(req, (const char *)app_js_start, app_js_end - app_js_start);
	return ESP_OK;
}

static httpd_handle_t http_server_configure(http_server_uri_array_t uris, size_t uris_length){
	httpd_config_t config = HTTPD_DEFAULT_CONFIG(); // Generate the default configuration
	
	 http_server_monitor_queue_handle = xQueueCreate(3, sizeof(http_server_queue_message_t));
    if (http_server_monitor_queue_handle == NULL) {
        ESP_LOGE(TAG, "Error: No se pudo crear la cola del monitor HTTP");
        return NULL;
    }

    xTaskCreatePinnedToCore(&http_server_monitor, "http_server_monitor",
                            HTTP_SERVER_MONITOR_STACK_SIZE, NULL,
                            HTTP_SERVER_MONITOR_PRIORITY, &task_http_server_monitor,
                            HTTP_SERVER_MONITOR_CORE_ID);

	config.core_id = HTTP_SERVER_TASK_CORE_ID; // The core that the HTTP server will run on
	config.task_priority = HTTP_SERVER_TASK_PRIORITY; // Adjust the default priority to 1 less than the wifi application task
	config.stack_size = HTTP_SERVER_TASK_STACK_SIZE; // Bump up the stack size (default is 4096)
	config.max_uri_handlers = 20; // Increase uri handlers
	config.recv_wait_timeout = 10; // Increase the timeout limits
	config.send_wait_timeout = 10;

	ESP_LOGI(TAG,
			"http_server_configure: Starting server on port: '%d' with task priority: '%d'",
			config.server_port,
			config.task_priority);

	if (httpd_start(&http_server_handle, &config) == ESP_OK) // Start the httpd server
	{
		ESP_LOGI(TAG, "http_server_configure: Registering URI handlers");
		
		httpd_uri_t index_html = { // register index.html handler
				.uri = "/",
				.method = HTTP_GET,
				.handler = http_server_index_html_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &index_html);

		httpd_uri_t lectura_pot_html = { // register lectura_pot.html handler
				.uri = "/lectura_pot",
				.method = HTTP_GET,
				.handler = http_server_lectura_pot_html_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &lectura_pot_html);

		httpd_uri_t lectura_temp_html = { // register lectura_temp.html handler
				.uri = "/lectura_temp",
				.method = HTTP_GET,
				.handler = http_server_lectura_temp_html_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &lectura_temp_html);

		httpd_uri_t led_rgb_html = { // register led_rgb.html handler
				.uri = "/led_rgb",
				.method = HTTP_GET,
				.handler = http_server_led_rgb_html_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &led_rgb_html);

		httpd_uri_t credenciales_html = { // register credenciales.html handler
				.uri = "/credenciales",
				.method = HTTP_GET,
				.handler = http_server_credenciales_html_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &credenciales_html);

		httpd_uri_t app_css = { // register app.css handler
				.uri = "/styles.css",
				.method = HTTP_GET,
				.handler = http_server_app_css_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &app_css);

		httpd_uri_t app_js = {  // register app.js handler
				.uri = "/scripts.js",
				.method = HTTP_GET,
				.handler = http_server_app_js_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &app_js);

		for (size_t i = 0; i < uris_length; ++i) {
        	httpd_register_uri_handler(http_server_handle, &(uris[i].uri));
    	}
		
		return http_server_handle;
	}

	return NULL;
}

void http_server_start(http_server_uri_array_t uris, size_t uris_length) {
    if (http_server_handle == NULL) {
        if (uris == NULL || uris_length == 0) {
			ESP_LOGI(TAG, "http_server_start: starting HTTP server with default URIs");
            http_server_handle = http_server_configure(NULL, 0);
        } else {
            http_server_handle = http_server_configure(uris, uris_length);
        }
    }
}

void http_server_stop(void){
	if (http_server_handle){
		httpd_stop(http_server_handle);
		ESP_LOGI(TAG, "http_server_stop: stopping HTTP server");
		http_server_handle = NULL;
	}
	if (task_http_server_monitor){
		vTaskDelete(task_http_server_monitor);
		ESP_LOGI(TAG, "http_server_stop: stopping HTTP server monitor");
		task_http_server_monitor = NULL;
	}
}

BaseType_t http_server_monitor_send_message(http_server_message_e msgID)
{
	http_server_queue_message_t msg;
	msg.msgID = msgID;
	return xQueueSend(http_server_monitor_queue_handle, &msg, portMAX_DELAY);
}

void http_server_fw_update_reset_callback(void *arg)
{
	ESP_LOGI(TAG, "http_server_fw_update_reset_callback: Timer timed-out, restarting the device");
	esp_restart();
}

httpd_handle_t http_server_get_handle(void) {
    return http_server_handle;
}