#include <stdio.h>
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "server/include/server.h"
#include "wifi_manager/include/wifi_manager.h"
#include "cJSON.h"
#include "peripherals.h"

#define TXD_PIN (GPIO_NUM_1)
#define RXD_PIN (GPIO_NUM_3)
#define UART_PORT UART_NUM_0

static const int RX_BUF_SIZE = 1024;
static volatile float last_T = 0.0;
static SemaphoreHandle_t temp_mutex; // Mutex para acceso seguro
static volatile bool temp_print_enabled = true; // Habilitar impresión por defecto
static SemaphoreHandle_t print_mutex; // Mutex para acceso seguro
static TimerHandle_t print_timer; // Timer para impresión periódica

static const char *TAG = "main";

typedef struct {
    const char *command;
    void (*handler)();
} Command;

typedef struct {
    float threshold_R[2];
    float threshold_G[2];
    float threshold_B[2];
} Thresholds_Color;

static Thresholds_Color current_thresholds = {
    .threshold_R = {0, 0},
    .threshold_G = {0, 0},
    .threshold_B = {0, 0}
};
SemaphoreHandle_t thresholds_mutex;

int sendData(const char* data)
{
    static const char *TAG = "SEND_DATA";
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_PORT, data, len);
    ESP_LOGI(TAG, "Wrote %d bytes", txBytes);
    return txBytes;
}

void setup_range_RGB_handle(const char color, float down, float up) {

    if (xSemaphoreTake(thresholds_mutex, portMAX_DELAY) != pdTRUE) {
        sendData("Error: No se pudo acceder al mutex\n");
        return;
    }

    if (strcmp(color, "R") == 0) {
        current_thresholds.threshold_R[0] = down;
        current_thresholds.threshold_R[1] = up;
    } else if (strcmp(color, "G") == 0) {
        current_thresholds.threshold_G[0] = down;
        current_thresholds.threshold_G[1] = up;
    } else if (strcmp(color, "B") == 0) {
        current_thresholds.threshold_B[0] = down;
        current_thresholds.threshold_B[1] = up;
    } else {
        xSemaphoreGive(thresholds_mutex);
        sendData("Error: Color no válido\n");
        return;
    }
    xSemaphoreGive(thresholds_mutex);
    sendData("Updating RGB LED range\n");
}

static void ntc_temp_rgb_task(void *arg) {
    NTC_Config ntc_config;
    ADC_Config adc_config;

    config_adc_unit adc_uint_conf = adc_init_adc_unit(ADC_UNIT_NTC);
    NTC_ADC_init(&adc_config, &ntc_config, adc_uint_conf);
    RGB_LED rgb_led_temp;

    setup_rgb_temp(&rgb_led_temp);
    rgb_led_set_duty(&rgb_led_temp, 0, 0, 0);

    int current_r = 0;
    int current_g = 0;
    int current_b = 0;

    int raw;
    float voltage;
    float R;
    float T;

    Thresholds_Color received_thresholds;

    while (1) {
        
        if (xSemaphoreTake(thresholds_mutex, portMAX_DELAY) == pdTRUE) {
            received_thresholds = current_thresholds;
            xSemaphoreGive(thresholds_mutex);
        }

        raw = read_adc_raw(&adc_config);
        voltage = adc_raw_to_voltage(&adc_config, raw);

        printf("Raw: %d, Voltage: %.2f V\n", raw, voltage);

        R = R_NTC(&ntc_config, voltage);
        T = T_NTC(&ntc_config, R);

        if (xSemaphoreTake(temp_mutex, portMAX_DELAY) == pdTRUE) {
            last_T = T;
            xSemaphoreGive(temp_mutex);
        }

        if (T >= received_thresholds.threshold_R[0] && T <= received_thresholds.threshold_R[1]) {
            current_r = 100;
        } else {
            current_r = 0;
        }
        if (T >= received_thresholds.threshold_G[0] && T <= received_thresholds.threshold_G[1]) {
            current_g = 100;
        } else {
            current_g = 0;
        }
        if (T >= received_thresholds.threshold_B[0] && T <= received_thresholds.threshold_B[1]) {
            current_b = 100;
        } else {
            current_b = 0;
        }
        rgb_led_set_color(&rgb_led_temp, current_r, current_g, current_b);
        printf("R_NTC: %.2f, T_NTC: %.2f\n", R, T);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void setup_range_RGB_preprocessed(char *arg) {
    char *color = strtok(arg, "$");
    char *down = strtok(NULL, "$");
    char *up = strtok(NULL, "$");

    if (color == NULL || down == NULL || up == NULL) {
        sendData("Error: Argumentos insuficientes\n");
        return;
    }
    float new_down = atof(down);
    float new_up = atof(up);
    setup_range_RGB_handle(color, new_down, new_up);
}

void temp_on(char *arg) {
    (void)arg;
     if (xSemaphoreTake(print_mutex, portMAX_DELAY) == pdTRUE) {
        temp_print_enabled = true;
        xTimerStart(print_timer, 0); // Iniciar/Reiniciar timer
        xSemaphoreGive(print_mutex);
        sendData("TEMP prints ENABLED (every 2s)\n");
    }
}

void temp_off(char *arg) {
    (void)arg;
      if (xSemaphoreTake(print_mutex, portMAX_DELAY) == pdTRUE) {
        temp_print_enabled = false;
        xTimerStop(print_timer, 0); // Detener timer
        xSemaphoreGive(print_mutex);
        sendData("TEMP prints DISABLED\n");
    }
}

Command command_table[] = {
    { "#SET_RANGE", setup_range_RGB_preprocessed},\
    {"#TEMP_ON", temp_on},\
    {"#TEMP_OFF",temp_off},\
    {NULL, NULL} 
};

void process_command(const char *input) {
    char input_copy[256]; // Copia de la entrada
    strncpy(input_copy, input, sizeof(input_copy));
    input_copy[sizeof(input_copy) - 1] = '\0'; // Aseguramos el fin de la cadena

    char *command;
    char *arg;
    command = strtok(input_copy, "$");
    arg = strtok(NULL, "");

    if (command == NULL) {
        printf("Error: No se encontró un comando\n");
        return;
    }

    for (int i = 0; command_table[i].command != NULL; i++) {
        if (strcmp(command, command_table[i].command) == 0) {
            command_table[i].handler(arg);
            if (arg != NULL) {
                printf("Command '%s' processed with argument '%s'\n", command, arg);
            } else {
                printf("Command '%s' processed with no arguments\n", command);
            }
            return;
        }
    }
    printf("Error: Command '%s' not found\n", command);
}

static void rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE + 1);

    if (data == NULL) {
        ESP_LOGE(RX_TASK_TAG, "Error al asignar memoria para el buffer UART");
        vTaskDelete(NULL); // Finaliza la tarea si no hay memoria
    }

    while (1) {
        const int rxBytes = uart_read_bytes(UART_PORT, data, RX_BUF_SIZE, 1000 / portTICK_PERIOD_MS);
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
            ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
            sendData("Data received\n");
            sendData((char*) data);
            process_command((char*) data);
        }
    }
    free(data);
}

void print_temp_callback(TimerHandle_t xTimer) {
    if (xSemaphoreTake(temp_mutex, portMAX_DELAY) == pdTRUE) {
        float current_T = last_T; // Leer última temperatura
        xSemaphoreGive(temp_mutex);

        if (temp_print_enabled) {
            char buffer[50];
            snprintf(buffer, sizeof(buffer), "Temp: %.2f°C\n", current_T);
            sendData(buffer);
        }
    }
}

static esp_err_t mi_handler_1(httpd_req_t *req) {
    httpd_resp_send(req, "Respuesta de mi_handler_1", HTTPD_RESP_USE_STRLEN);
	httpd_resp_set_hdr(req, "Connection", "close");
    return ESP_OK;
}

static esp_err_t get_values(httpd_req_t *req) {
    ESP_LOGI(TAG, "/dhtSensor.json requested");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "temp", 30.1);
    cJSON_AddNumberToObject(root, "humidity", 40.5);
    char *json_string = cJSON_Print(root);
    cJSON_Delete(root); // Liberar la memoria del objeto JSON
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string, strlen(json_string));
    free(json_string); // Liberar la memoria de la cadena JSON
	httpd_resp_set_hdr(req, "Connection", "close");
    return ESP_OK;
}

static esp_err_t rgb_values_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "/rgb_values.json requested");

    char content[200];
    int total_len = 0;
    int cur_len = 0;
    int content_len = req->content_len;
    ESP_LOGI(TAG, "Content length: %d", content_len);

    
    while (total_len < content_len) { // Leer el body en un bucle hasta obtener todos los datos
        cur_len = httpd_req_recv(req, content + total_len, sizeof(content) - 1 - total_len);
        if (cur_len < 0) {
            if (cur_len == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive request body");
            return ESP_FAIL;
        }
        total_len += cur_len;
    }
    content[total_len] = '\0'; // Asegurarse de terminar en null

    ESP_LOGI(TAG, "Content: %s", content);

    cJSON *root = cJSON_Parse(content);
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *red_item = cJSON_GetObjectItem(root, "R");
    cJSON *green_item = cJSON_GetObjectItem(root, "G");
    cJSON *blue_item = cJSON_GetObjectItem(root, "B");

    if (!red_item || !green_item || !blue_item || !cJSON_IsNumber(red_item) || !cJSON_IsNumber(green_item) || !cJSON_IsNumber(blue_item)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid RGB values in JSON");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    int red = red_item->valueint;
    int green = green_item->valueint;
    int blue = blue_item->valueint;

    if (red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 || blue > 255) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "RGB values out of range (0-255)");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    cJSON_Delete(root);

    ESP_LOGI(TAG, "RGB values: red=%d, green=%d, blue=%d", red, green, blue);
    // Aquí podrías llamar a la función para cambiar el color del LED, por ejemplo:
    // rgb_led_set_color(red, green, blue);

    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}


http_server_uri_t uris[] = {
	{
		.uri = {
			.uri = "/ruta1",
			.method = HTTP_POST,
			.handler = rgb_values_handler,
			.user_ctx = NULL
		}
	},
	{
		.uri = {
			.uri = "/ruta2",
			.method = HTTP_GET,
			.handler = get_values,
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

    xTaskCreate(ntc_temp_rgb_task, "ntc_temp_rgb_task", 4096, NULL, configMAX_PRIORITIES - 3, NULL);
}