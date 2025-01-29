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

static const int RX_BUF_SIZE = 1024;

#define TXD_PIN (GPIO_NUM_1)
#define RXD_PIN (GPIO_NUM_3)

#define GPIO_INPUT_IO_0     0
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_0))
#define ESP_INTR_FLAG_DEFAULT 0

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

int current_led = RED;

int sendData(const char* data)
{
    static const char *TAG = "SEND_DATA";
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_NUM_0, data, len);
    ESP_LOGI(TAG, "Wrote %d bytes", txBytes);
    return txBytes;
}

void setup_range_RGB(char *arg) {
    // Extraer los argumentos
    char *color = strtok(arg, "$");
    char *down = strtok(NULL, "$");
    char *up = strtok(NULL, "$");

    // Validar que los argumentos no sean NULL
    if (color == NULL || down == NULL || up == NULL) {
        sendData("Error: Argumentos insuficientes\n");
        return;
    }

    // Obtener los umbrales actuales de la cola
    Thresholds_Color thresholds;
    if (xSemaphoreTake(thresholds_mutex, portMAX_DELAY) == pdTRUE) {
        thresholds = current_thresholds;
        xSemaphoreGive(thresholds_mutex);
    } else {
        sendData("Error: No se pudo acceder al mutex\n");
        return;
    }

    // Convertir los valores de down y up a float
    float new_down = atof(down);
    float new_up = atof(up);

    // Actualizar los umbrales según el color
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

void get_temp(){
    fprintf(stderr, "Getting temperature\n");
}

Command command_table[] = {
    { "#SET_RANGE", setup_range_RGB },\
    { "#GET_TEMP", get_temp},\
    {NULL, NULL} 
};

void process_command(const char *input) {
    char input_copy[256]; // Copia de la entrada
    strncpy(input_copy, input, sizeof(input_copy));
    input_copy[sizeof(input_copy) - 1] = '\0'; // Aseguramos el fin de la cadena

    //#SET_RANGE$B$0$100
    char *command;
    char *arg;
    command = strtok(input_copy, "$");
    arg = strtok(NULL, "");

    // Validamos que el comando no sea NULL
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
    uart_driver_install(UART_NUM_0, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &uart_config);
    uart_set_pin(UART_NUM_0, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

static void tx_task(void *arg)
{
    static const char *TX_TASK_TAG = "TX_TASK";
    esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);
    while (1) {
        sendData("Hello world from ESP32!");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

//region UART_RX_TASK
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
        const int rxBytes = uart_read_bytes(UART_NUM_0, data, RX_BUF_SIZE, 1000 / portTICK_PERIOD_MS);
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
    rgb_led_dimmer->red.gpio_num = 12;
    rgb_led_dimmer->red.channel = LEDC_CHANNEL_3;

    rgb_led_dimmer->green.gpio_num = 27;
    rgb_led_dimmer->green.channel = LEDC_CHANNEL_4;

    rgb_led_dimmer->blue.gpio_num = 26;
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
    rgb_led_temp->config.invert = 0;

    // Llamar a la función de inicialización
    rgb_led_init(rgb_led_temp);
    printf("RGB LED Temp configured\n");
}

void dimmer_RGB_task(void *arg) {
    RGB_LED rgb_led_dimmer;
    setup_rgb_dimmer(&rgb_led_dimmer);

    rgb_led_set_color(&rgb_led_dimmer, 0, 0, 0);

    config_adc_unit adc_uint_conf_dimmer = adc_init_adc_unit(ADC_UNIT_1);

    ADC_Config adc_config_dimmer;
    adc_config_dimmer.channel = ADC_CHANNEL_5;
    adc_config_dimmer.bitwidth = ADC_BITWIDTH_DEFAULT;
    adc_config_dimmer.atten = ADC_ATTEN_DB_12;

    adc_initialize(&adc_config_dimmer, adc_uint_conf_dimmer);
    static RGB_Values current_values_rgb = {0, 0, 0};   


    int raw_dimmer;
    
    while(1){
        raw_dimmer = read_adc_raw(&adc_config_dimmer);
        fprintf(stderr, "Raw: %d\n", raw_dimmer);
        if (current_led == RED)
        {
            current_values_rgb.red = raw_dimmer;
        }
        else if (current_led == GREEN)
        {
            current_values_rgb.green = raw_dimmer;
        }
        else
        {
            current_values_rgb.blue = raw_dimmer;
        }
        rgb_led_set_duty(&rgb_led_dimmer, current_values_rgb.red, current_values_rgb.green, current_values_rgb.blue);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void NTC_ADC_init(ADC_Config *adc_config, NTC_Config *ntc_config, config_adc_unit adc_uint_conf){
    adc_config->channel = ADC_CHANNEL_0;
    adc_config->bitwidth = ADC_BITWIDTH_DEFAULT;
    adc_config->atten = ADC_ATTEN_DB_12;

    ntc_config->b = 3200;
    ntc_config->R0 = 47;
    ntc_config->T0 = 298.15;
    ntc_config->R1 = 100;
    adc_initialize(adc_config, adc_uint_conf);
}

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    // Cambiar el color actual (ciclo entre 0, 1 y 2)
    current_led = (current_led + 1) % 3;
}

static void config_button(){
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = 1;
    gpio_config(&io_conf);

    //change gpio interrupt type for one pin
    gpio_set_intr_type(GPIO_INPUT_IO_0, GPIO_INTR_POSEDGE);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void*) GPIO_INPUT_IO_0);
}

static void ntc_temp_rgb_task(void *arg) {
    NTC_Config ntc_config;
    ADC_Config adc_config;

    config_adc_unit adc_uint_conf = adc_init_adc_unit(ADC_UNIT_2);

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

        char buffer[50];
        sprintf(buffer, "Temp: %.2f\n", T);
        sendData(buffer);
        sendData("\n");

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

void app_main(void)
{   
    config_button();
    thresholds_mutex = xSemaphoreCreateMutex();
    if (thresholds_mutex == NULL) {
        ESP_LOGE("APP_MAIN", "No se pudo crear el mutex");
        return;
    }

    init();
    xTaskCreate(rx_task, "uart_rx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, NULL);

    //xTaskCreate(tx_task, "uart_tx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 2, NULL);
    xTaskCreate(ntc_temp_rgb_task, "ntc_temp_rgb_task", 4096, NULL, configMAX_PRIORITIES - 3, NULL);
    xTaskCreate(dimmer_RGB_task, "dimmer_rbg_task", 4096, NULL, configMAX_PRIORITIES - 4, NULL);
}