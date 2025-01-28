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

static const int RX_BUF_SIZE = 1024;

#define TXD_PIN (GPIO_NUM_4)
#define RXD_PIN (GPIO_NUM_5)

typedef struct {
    const char *command;
    void (*handler)();
} Command;

void setup_range_R(){
    fprintf(stderr, "Setting up RGB LED range\n");
}

void get_temp(){
    fprintf(stderr, "Getting temperature\n");
}

Command command_table[] = {
    { "#RED_MAX", setup_range_R },\
    { "#GET_TEMP", get_temp},\
    {NULL, NULL} 
};

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
    uart_driver_install(UART_NUM_0, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &uart_config);
    uart_set_pin(UART_NUM_0, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

int sendData(const char* logName, const char* data)
{
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_NUM_0, data, len);
    ESP_LOGI(logName, "Wrote %d bytes", txBytes);
    return txBytes;
}

static void tx_task(void *arg)
{
    static const char *TX_TASK_TAG = "TX_TASK";
    esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);
    while (1) {
        sendData(TX_TASK_TAG, "Hello world");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

static void rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE + 1);
    while (1) {
        const int rxBytes = uart_read_bytes(UART_NUM_0, data, RX_BUF_SIZE, 1000 / portTICK_PERIOD_MS);
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
            ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
        }
    }
    free(data);
}

static void setup_rgb_temp(RGB_LED *rgb_led_temp) {
    // Usar el operador de acceso indirecto para llenar los campos
    rgb_led_temp->red.gpio_num = 21;
    rgb_led_temp->red.channel = LEDC_CHANNEL_0;

    rgb_led_temp->green.gpio_num = 19;
    rgb_led_temp->green.channel = LEDC_CHANNEL_1;

    rgb_led_temp->blue.gpio_num = 18;
    rgb_led_temp->blue.channel = LEDC_CHANNEL_2;

    rgb_led_temp->config.mode = LEDC_LOW_SPEED_MODE;
    rgb_led_temp->config.timer = LEDC_TIMER_0;
    rgb_led_temp->config.duty_res = LEDC_TIMER_13_BIT;
    rgb_led_temp->config.frequency = 4000;

    // Llamar a la función de inicialización
    rgb_led_init(rgb_led_temp);
    printf("RGB LED Temp configured\n");
}

void NTC_ADC_init(ADC_Config *adc_config, NTC_Config *ntc_config){
    adc_config->channel = ADC_CHANNEL_0;
    adc_config->bitwidth = ADC_BITWIDTH_DEFAULT;
    adc_config->atten = ADC_ATTEN_DB_12;

    ntc_config->b = 3200;
    ntc_config->R0 = 47;
    ntc_config->T0 = 298.15;
    ntc_config->R1 = 100;

    config_adc_unit adc_uint_conf = adc_init_adc_unit(ADC_UNIT_2);
    adc_initialize(adc_config, adc_uint_conf);
}

typedef struct {
    float threshold_low;
    float threshold_high;
} Thresholds_Color;

QueueHandle_t threshold_queue_red;
QueueHandle_t threshold_queue_green;
QueueHandle_t threshold_queue_blue;

static void ntc_temp_rgb_task(void *arg){
    NTC_Config ntc_config;
    ADC_Config adc_config;
    NTC_ADC_init(&adc_config, &ntc_config);

    RGB_LED rgb_led_temp;
    setup_rgb_temp(&rgb_led_temp);
    rgb_led_set_duty(&rgb_led_temp, 0, 0, 0);

    Thresholds_Color threshold_blue = {0, 35}; 
    Thresholds_Color threshold_green = {25.0, 35.0};
    Thresholds_Color threshold_red = {30, 45};

    Thresholds_Color received_thresholds;

    int current_r = 0;
    int current_g = 0;
    int current_b = 0;

    int raw;
    float voltage;
    float R;
    float T;
    while (1){

        /*
        if (xQueueReceive(threshold_queue_red, &received_thresholds, 0) == pdTRUE) {
            threshold_red = received_thresholds;
            printf("Thresholds updated in temperature task: low = %.2f, high = %.2f\n",
                   threshold_red.threshold_low, threshold_red.threshold_high);
        }

        if (xQueueReceive(threshold_queue_green, &received_thresholds, 0) == pdTRUE) {
            threshold_green = received_thresholds;
            printf("Thresholds updated in temperature task: low = %.2f, high = %.2f\n",
                   threshold_green.threshold_low, threshold_green.threshold_high);
        }

        if (xQueueReceive(threshold_queue_blue, &received_thresholds, 0) == pdTRUE) {
            threshold_blue = received_thresholds;
            printf("Thresholds updated in temperature task: low = %.2f, high = %.2f\n",
                   threshold_blue.threshold_low, threshold_blue.threshold_high);
        }
        */

        raw = read_adc_raw(&adc_config);
        voltage = adc_raw_to_voltage(&adc_config, raw);
        printf("Raw: %d, Voltage: %.2f V\n", raw, voltage);
        R = R_NTC(&ntc_config, voltage);
        T = T_NTC(&ntc_config, R);

        if (T > threshold_red.threshold_low && T < threshold_red.threshold_high){
            current_r = 100;
        } else {
            current_r = 0;
        }

        if (T > threshold_green.threshold_low && T < threshold_green.threshold_high){
            current_g = 100;
        } else {
            current_g = 0;
        }

        if (T > threshold_blue.threshold_low && T < threshold_blue.threshold_high){
            current_b = 100;
        } else {
            current_b = 0;
        }

        rgb_led_set_color(&rgb_led_temp, current_r, current_g, current_b);

        printf("R_NTC: %.2f, T_NTC: %.2f\n", R, T);
        vTaskDelay(pdMS_TO_TICKS(1000));
    
    }
}

void process_command(const char *input) {
    char input_copy[256]; // Copia de la entrada
    strncpy(input_copy, input, sizeof(input_copy));
    input_copy[sizeof(input_copy) - 1] = '\0'; // Aseguramos el fin de la cadena

    char *command;
    char *arg;
    command = strtok(input_copy, "$");
    arg = strtok(NULL, "$");

    // Validamos que el comando no sea NULL
    if (command == NULL) {
        printf("Error: No se encontró un comando\n");
        return;
    }

    // Iteramos por la tabla de comandos para buscar una coincidencia
    for (int i = 0; command_table[i].command != NULL; i++) {
        if (strcmp(command, command_table[i].command) == 0) {
            command_table[i].handler(arg);

            // Si existe un argumento, lo mostramos
            if (arg != NULL) {
                printf("Command '%s' processed with argument '%s'\n", command, arg);
            } else {
                printf("Command '%s' processed with no arguments\n", command);
            }
            return;
        }
    }

    // Si no se encontró el comando
    printf("Error: Command '%s' not found\n", command);
}



void app_main(void)
{   

    init();
    process_command("#RED_MAX$40$");
    xTaskCreate(rx_task, "uart_rx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, NULL);
    xTaskCreate(tx_task, "uart_tx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 2, NULL);
    xTaskCreate(ntc_temp_rgb_task, "ntc_temp_rgb_task", 4096, NULL, configMAX_PRIORITIES - 3, NULL);
}