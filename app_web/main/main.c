#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"
#include "PWM_Control/include/pwm_control.h"
#include "ADC_lib/include/adc_lib.h"
#include "NTC_lib/include/ntc_lib.h"
#include "string.h"
#include "freertos/semphr.h"
#include "server/include/server.h"
#include "wifi_manager/include/wifi_manager.h"
#include "nvs_flash.h"
#include "cJSON.h"

static const char *TAG = "main";
RGB_LED rgb_led;
RGB_LED rgb_led_temp;

static const int RX_BUF_SIZE = 1024;

#define TXD_PIN (GPIO_NUM_1)
#define RXD_PIN (GPIO_NUM_3)

#define GPIO_INPUT_IO_0     0
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_0))
#define ESP_INTR_FLAG_DEFAULT 0
#define GPIO_R_TEMP 21
#define GPIO_G_TEMP 19
#define GPIO_B_TEMP 18

#define GPIO_R_DIMMER 12
#define GPIO_G_DIMMER 27
#define GPIO_B_DIMMER 26

#define ADC_UNIT_NTC ADC_UNIT_1
#define ADC_UNIT_DIMMER ADC_UNIT_1

#define CONFIG_ADC_CHANNEL_NTC ADC_CHANNEL_4
#define CONFIG_ADC_CHANNEL_DIMMER ADC_CHANNEL_5

#define UART_PORT UART_NUM_0

#define NTC_B 3200
#define NTC_R0 47
#define NTC_T0 298.15
#define NTC_R1 100

typedef struct {
    int on_time;  // Tiempo encendido en milisegundos
    int off_time; // Tiempo apagado en milisegundos
} BlinkTiming;

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

static BlinkTiming blink_timing = {500, 500}; // Valores por defecto
static SemaphoreHandle_t blink_timing_mutex;

static volatile float last_T = 0.0;
static SemaphoreHandle_t temp_mutex; // Mutex para acceso seguro
static volatile bool temp_print_enabled = true; // Habilitar impresión por defecto
static SemaphoreHandle_t print_mutex; // Mutex para acceso seguro
static TimerHandle_t print_timer; // Timer para impresión periódica

uint32_t dimmer_value;

config_adc_unit adc_uint_conf; 

enum current_led
{
    RED,
    GREEN,
    BLUE
};

typedef struct RGB_Values
{
    int red;
    int green;
    int blue;
} RGB_Values;

static RGB_Values led_temp_color = {100, 100, 100}; // Valores iniciales
static SemaphoreHandle_t led_temp_color_mutex;   // Semáforo para proteger shared_colors

int current_led = RED;

int sendData(const char* data)
{
    static const char *TAG = "SEND_DATA";
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_PORT, data, len);
    ESP_LOGI(TAG, "Wrote %d bytes", txBytes);
    return txBytes;
}

void setup_range_RGB(char *arg) {
    char *color = strtok(arg, "$");
    char *down = strtok(NULL, "$");
    char *up = strtok(NULL, "$");

    if (color == NULL || down == NULL || up == NULL) {
        sendData("Error: Argumentos insuficientes\n");
        return;
    }

    Thresholds_Color thresholds;
    if (xSemaphoreTake(thresholds_mutex, portMAX_DELAY) == pdTRUE) {
        thresholds = current_thresholds;
        xSemaphoreGive(thresholds_mutex);
    } else {
        sendData("Error: No se pudo acceder al mutex\n");
        return;
    }

    float new_down = atof(down);
    float new_up = atof(up);

    if (strcmp(color, "R") == 0) {
        thresholds.threshold_R[0] = new_down;
        thresholds.threshold_R[1] = new_up;
    } else if (strcmp(color, "G") == 0) {
        thresholds.threshold_G[0] = new_down;
        thresholds.threshold_G[1] = new_up;
    } else if (strcmp(color, "B") == 0) {
        thresholds.threshold_B[0] = new_down;
        thresholds.threshold_B[1] = new_up;
    } else {
        sendData("Error: Color no válido\n");
        return;
    }

    if (xSemaphoreTake(thresholds_mutex, portMAX_DELAY) == pdTRUE) {
        current_thresholds = thresholds;
        xSemaphoreGive(thresholds_mutex);
        sendData("Updating RGB LED range\n");
    } else {
        sendData("Error: No se pudo actualizar los umbrales\n");
    }
}

void blink_time_uart(char *arg) {
    char *on_time = strtok(arg, "$");
    char *off_time = strtok(NULL, "$");

    if (on_time == NULL || off_time == NULL) {
        sendData("Error: Argumentos insuficientes\n");
        return;
    }
    float new_on = atof(on_time);
    float new_off = atof(off_time);

    if (xSemaphoreTake(blink_timing_mutex, portMAX_DELAY) == pdTRUE) {
        blink_timing.on_time = new_on;
        blink_timing.off_time = new_off;
        xSemaphoreGive(blink_timing_mutex);
    }
    sendData("Blink time updated\n");
    
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

Command command_table[] = {
    { "#SET_RANGE", setup_range_RGB },\
    {"#TEMP_ON", temp_on},\
    {"#TEMP_OFF",temp_off},\
    {"#BLINK_TIME", blink_time_uart},\
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

void init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_PORT, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_PORT, &uart_config);
    uart_set_pin(UART_PORT, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
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
//endregion

static void setup_rgb_dimmer(RGB_LED *rgb_led_dimmer) {
    rgb_led_dimmer->red.gpio_num = GPIO_R_DIMMER;
    rgb_led_dimmer->red.channel = LEDC_CHANNEL_3;

    rgb_led_dimmer->green.gpio_num = GPIO_G_DIMMER;
    rgb_led_dimmer->green.channel = LEDC_CHANNEL_4;

    rgb_led_dimmer->blue.gpio_num = GPIO_B_DIMMER;
    rgb_led_dimmer->blue.channel = LEDC_CHANNEL_5;

    rgb_led_dimmer->config.mode = LEDC_LOW_SPEED_MODE;
    rgb_led_dimmer->config.timer = LEDC_TIMER_1;
    rgb_led_dimmer->config.duty_res = LEDC_TIMER_13_BIT;
    rgb_led_dimmer->config.frequency = 4000;
    rgb_led_dimmer->config.invert = 1;

    rgb_led_init(rgb_led_dimmer);
    printf("RGB LED Dimmer configured\n");
}

static void setup_rgb_temp(RGB_LED *rgb_led_temp) {
    rgb_led_temp->red.gpio_num = GPIO_R_TEMP;
    rgb_led_temp->red.channel = LEDC_CHANNEL_0;

    rgb_led_temp->green.gpio_num = GPIO_G_TEMP;
    rgb_led_temp->green.channel = LEDC_CHANNEL_1;

    rgb_led_temp->blue.gpio_num = GPIO_B_TEMP;
    rgb_led_temp->blue.channel = LEDC_CHANNEL_2;

    rgb_led_temp->config.mode = LEDC_LOW_SPEED_MODE;
    rgb_led_temp->config.timer = LEDC_TIMER_0;
    rgb_led_temp->config.duty_res = LEDC_TIMER_13_BIT;
    rgb_led_temp->config.frequency = 4000;
    rgb_led_temp->config.invert = 1;

    rgb_led_init(rgb_led_temp);
    printf("RGB LED Temp configured\n");
}

void dimmer_RGB_task(void *arg) {
    RGB_LED rgb_led_dimmer;
    setup_rgb_dimmer(&rgb_led_dimmer);
    rgb_led_set_color(&rgb_led_dimmer, 0, 0, 0);

    //config_adc_unit adc_uint_conf_dimmer = adc_init_adc_unit(ADC_UNIT_DIMMER);

    ADC_Config adc_config_dimmer;
    adc_config_dimmer.channel = CONFIG_ADC_CHANNEL_DIMMER;
    adc_config_dimmer.bitwidth = ADC_BITWIDTH_DEFAULT;
    adc_config_dimmer.atten = ADC_ATTEN_DB_12;

    adc_initialize(&adc_config_dimmer, adc_uint_conf);
    static RGB_Values current_values_rgb = {0, 0, 0};   
    int raw_dimmer;
    
    while(1){
        raw_dimmer = read_adc_raw(&adc_config_dimmer);
        dimmer_value = raw_dimmer;
        fprintf(stderr, "Raw: %d\n", raw_dimmer);
        if (current_led == RED)
        {
            current_values_rgb.red = raw_dimmer;
        }
        else if (current_led == GREEN)
        {
            current_values_rgb.green = raw_dimmer;
        }else
        {
            current_values_rgb.blue = raw_dimmer;
        }
        rgb_led_set_duty(&rgb_led_dimmer, current_values_rgb.red, current_values_rgb.green, current_values_rgb.blue);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void NTC_ADC_init(ADC_Config *adc_config, NTC_Config *ntc_config, config_adc_unit adc_uint_conf){
    adc_config->channel = CONFIG_ADC_CHANNEL_NTC;
    adc_config->bitwidth = ADC_BITWIDTH_DEFAULT;
    adc_config->atten = ADC_ATTEN_DB_12;
    ntc_config->b = NTC_B;
    ntc_config->R0 = NTC_R0;
    ntc_config->T0 = NTC_T0;
    ntc_config->R1 = NTC_R1;
    adc_initialize(adc_config, adc_uint_conf);
}

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    current_led = (current_led + 1) % 3;
}

static void config_button(){
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = 1;
    gpio_config(&io_conf);
    gpio_set_intr_type(GPIO_INPUT_IO_0, GPIO_INTR_POSEDGE);
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void*) GPIO_INPUT_IO_0);
}

static void ntc_temp_rgb_task(void *arg) {
    NTC_Config ntc_config;
    ADC_Config adc_config;

    NTC_ADC_init(&adc_config, &ntc_config, adc_uint_conf);

    int raw;
    float voltage;
    float R;
    float T;

    while (1) {
        raw = read_adc_raw(&adc_config);
        voltage = adc_raw_to_voltage(&adc_config, raw);

        printf("Raw: %d, Voltage: %.2f V\n", raw, voltage);

        R = R_NTC(&ntc_config, voltage);
        T = T_NTC(&ntc_config, R);
        if (xSemaphoreTake(temp_mutex, portMAX_DELAY) == pdTRUE) {
            last_T = T;
            xSemaphoreGive(temp_mutex);
        }
        printf("R_NTC: %.2f, T_NTC: %.2f\n", R, T);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

//----------------------------------------------------
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
    int red_percent = (red * 100) / 255;
    int green_percent = (green * 100) / 255;
    int blue_percent = (blue * 100) / 255;

    if (xSemaphoreTake(led_temp_color_mutex, portMAX_DELAY) == pdTRUE) {
        led_temp_color.red = red;
        led_temp_color.green = green;
        led_temp_color.blue = blue;
        xSemaphoreGive(led_temp_color_mutex);
    } else {
        ESP_LOGE(TAG, "No se pudo tomar el semáforo en rgb_values_handler");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal server error");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    rgb_led_set_color(&rgb_led_temp, led_temp_color.red, led_temp_color.green, led_temp_color.blue);


    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"success\"}", HTTPD_RESP_USE_STRLEN);

    httpd_resp_set_hdr(req, "Connection", "close");
    return ESP_OK;
}

static esp_err_t get_values(httpd_req_t *req) {
    ESP_LOGI(TAG, "/dhtSensor.json requested");
    float current_temp = 0.0;
    if(xSemaphoreTake(temp_mutex, portMAX_DELAY) == pdTRUE) {
        current_temp = last_T;
        xSemaphoreGive(temp_mutex);
    }
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "temp", current_temp);
    char *json_string = cJSON_Print(root);
    cJSON_Delete(root);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string, strlen(json_string));
    free(json_string);
    httpd_resp_set_hdr(req, "Connection", "close");
    return ESP_OK;
}

static esp_err_t get_dimmer_value_handler(httpd_req_t *req)
{
    // Obtener el valor del dimmer desde el ADC
    // Crear un objeto JSON con el valor
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "dimmer", dimmer_value);

    // Convertir a string JSON
    const char *json_response = cJSON_Print(root);

    // Enviar respuesta
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_response, strlen(json_response));

    // Liberar memoria
    cJSON_Delete(root);
    free((void *)json_response);
    httpd_resp_set_hdr(req, "Connection", "close");

    return ESP_OK;
}

void blink_led_task(void *arg) {
    RGB_LED *rgb_led_temp = (RGB_LED *)arg;
    BlinkTiming timing;
    RGB_Values colors;

    while (1) {
        if (xSemaphoreTake(blink_timing_mutex, portMAX_DELAY) == pdTRUE) {
            timing = blink_timing;
            xSemaphoreGive(blink_timing_mutex);
        }

        if (xSemaphoreTake(led_temp_color_mutex, portMAX_DELAY) == pdTRUE) {
            colors = led_temp_color;
            xSemaphoreGive(led_temp_color_mutex);
        } else {
            ESP_LOGE(TAG, "No se pudo tomar el semáforo en blink_led_task");
        }

        rgb_led_set_color(rgb_led_temp, led_temp_color.red, led_temp_color.green, led_temp_color.blue);
        vTaskDelay(pdMS_TO_TICKS(timing.on_time));

        rgb_led_set_color(rgb_led_temp, 0, 0, 0); // Apagar el LED
        vTaskDelay(pdMS_TO_TICKS(timing.off_time));
    }
}

static esp_err_t blink_led_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "/blink_led requested");

    char content[200];
    int total_len = 0;
    int cur_len = 0;
    int content_len = req->content_len;
    ESP_LOGI(TAG, "Content length: %d", content_len);

    while (total_len < content_len) {
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
    content[total_len] = '\0';

    ESP_LOGI(TAG, "Content: %s", content);

    cJSON *root = cJSON_Parse(content);
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *on_time_item = cJSON_GetObjectItem(root, "on_time");
    cJSON *off_time_item = cJSON_GetObjectItem(root, "off_time");

    if (!on_time_item || !off_time_item || !cJSON_IsNumber(on_time_item) || !cJSON_IsNumber(off_time_item)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid timing values in JSON");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    int on_time = on_time_item->valueint;
    int off_time = off_time_item->valueint;

    if (on_time < 0 || off_time < 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Timing values must be positive");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    cJSON_Delete(root);

    if (xSemaphoreTake(blink_timing_mutex, portMAX_DELAY) == pdTRUE) {
        blink_timing.on_time = on_time;
        blink_timing.off_time = off_time;
        xSemaphoreGive(blink_timing_mutex);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"success\"}", HTTPD_RESP_USE_STRLEN);
    httpd_resp_set_hdr(req, "Connection", "close");
    return ESP_OK;
}

http_server_uri_t uris[] = {
	{
		.uri = {
			.uri = "/actu_led",
			.method = HTTP_POST,
			.handler = rgb_values_handler,
			.user_ctx = NULL
		}
	},
	{
		.uri = {
			.uri = "/temp_adc",
			.method = HTTP_GET,
			.handler = get_values,
			.user_ctx = NULL
		}
	},

    {
        .uri = {
        .uri       = "/dimmer",
        .method    = HTTP_GET,
        .handler   = get_dimmer_value_handler,
        .user_ctx  = NULL
        }
    },

    {
        .uri = {
            .uri = "/blink_led",
            .method = HTTP_POST,
            .handler = blink_led_handler,
            .user_ctx = NULL
        }
    },
};

size_t uris_length = sizeof(uris) / sizeof(uris[0]);

void app_main(void)
{   
    
    setup_rgb_temp(&rgb_led_temp);

    adc_uint_conf = adc_init_adc_unit(ADC_UNIT_NTC);
    config_button();
    thresholds_mutex = xSemaphoreCreateMutex();
    temp_mutex = xSemaphoreCreateMutex();
    print_mutex = xSemaphoreCreateMutex();
    blink_timing_mutex = xSemaphoreCreateMutex();

    led_temp_color_mutex = xSemaphoreCreateMutex();
    if (led_temp_color_mutex == NULL) {
        ESP_LOGE(TAG, "No se pudo crear el semáforo shared_colors_mutex");
        return;
    }

    print_timer = xTimerCreate(
        "PrintTempTimer",
        pdMS_TO_TICKS(2000), // 2 segundos
        pdTRUE, // Auto-reload
        NULL,
        print_temp_callback
    );
    xTimerStart(print_timer, 0);

    if (print_mutex == NULL || print_timer == NULL) {
        ESP_LOGE("APP_MAIN", "Error al crear recursos");
        return;
    }

    xTimerStart(print_timer, 0); // Iniciar timer

    if (thresholds_mutex == NULL) {
        ESP_LOGE("APP_MAIN", "No se pudo crear el mutex");
        return;
    }

    init();

    esp_err_t ret = nvs_flash_init(); // Initialize NVS
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
    wifi_manager_start();
	http_server_start(uris, uris_length);

    xTaskCreate(rx_task, "uart_rx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, NULL);
    xTaskCreate(ntc_temp_rgb_task, "ntc_temp_rgb_task", 4096, NULL, configMAX_PRIORITIES - 3, NULL);
    xTaskCreate(dimmer_RGB_task, "dimmer_rbg_task", 4096, NULL, configMAX_PRIORITIES - 4, NULL);
    xTaskCreate(blink_led_task, "blink_led_task", 4096, &rgb_led_temp, configMAX_PRIORITIES - 2, NULL);
}