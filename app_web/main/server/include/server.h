#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "sys/param.h"

#include "tasks_common.h"

typedef enum http_server_message
{
	HTTP_MSG_WIFI_CONNECT_INIT = 0,
	HTTP_MSG_WIFI_CONNECT_SUCCESS,
	HTTP_MSG_WIFI_CONNECT_FAIL,
	HTTP_MSG_OTA_UPDATE_SUCCESSFUL,
	HTTP_MSG_OTA_UPDATE_FAILED,
} http_server_message_e;

typedef struct http_server_queue_message
{
    http_server_message_e msgID;
} http_server_queue_message_t;

typedef struct {
    httpd_uri_t uri;
} http_server_uri_t;

typedef http_server_uri_t http_server_uri_array_t[];

void http_server_start(http_server_uri_array_t uris, size_t uris_length);
void http_server_stop(void);
