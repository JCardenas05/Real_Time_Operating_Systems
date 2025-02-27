#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"
#include "PWM_Control/include/pwm_control.h"
#include "ADC_lib/include/adc_lib.h"
#include "string.h"
#include "freertos/semphr.h"

static const int RX_BUF_SIZE = 1024;

#define TXD_PIN (GPIO_NUM_1)
#define RXD_PIN (GPIO_NUM_3)

#define GPIO_INPUT_IO_0     0
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_0))
#define ESP_INTR_FLAG_DEFAULT 0

#define UART_PORT UART_NUM_0

typedef struct {
    const char *command;
    void (*handler)();
} Command;


SemaphoreHandle_t thresholds_mutex;

static volatile float last_T = 0.0;
static volatile bool temp_print_enabled = true; // Habilitar impresión por defecto
static SemaphoreHandle_t print_mutex; // Mutex para acceso seguro
static TimerHandle_t print_timer; // Timer para impresión periódica

int current_led = 0;

int sendData(const char* data)
{
    static const char *TAG = "SEND_DATA";
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_PORT, data, len);
    ESP_LOGI(TAG, "Wrote %d bytes", txBytes);
    return txBytes;
}

/*
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
*/

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

Command command_table[] = { //setup_range_RGB
    { "#SET_RANGE", temp_off },\
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

void app_main(void)
{   
    config_button();
    print_mutex = xSemaphoreCreateMutex();

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
    xTaskCreate(rx_task, "uart_rx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, NULL);
}