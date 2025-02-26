#include "peripherals.h"
#include <stdio.h>
#include "driver/gpio.h"
#include "driver/ledc.h"

// Variables internas para los LEDs RGB
static RGB_LED rgb_led_temp;
static RGB_LED rgb_led_dimmer;

/* Definición de la variable global (declarada en el .h) */
int current_led = 0;

void setup_rgb_temp(RGB_LED *rgb_led_temp) {
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
    rgb_led_temp->config.invert = 0;

    rgb_led_init(rgb_led_temp);
    printf("RGB LED Temp configured\n");
}

void setup_rgb_dimmer(RGB_LED *rgb_led_dimmer) {
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

void NTC_ADC_init(ADC_Config *adc_config, NTC_Config *ntc_config, config_adc_unit adc_uint_conf) {
    adc_config->channel = CONFIG_ADC_CHANNEL_NTC;
    adc_config->bitwidth = ADC_BITWIDTH_DEFAULT;
    adc_config->atten = ADC_ATTEN_DB_12;
    
    ntc_config->b = NTC_B;
    ntc_config->R0 = NTC_R0;
    ntc_config->T0 = NTC_T0;
    ntc_config->R1 = NTC_R1;
    
    adc_initialize(adc_config, adc_uint_conf);
}

/**
 * @brief ISR para el botón.
 *
 * Incrementa la variable global current_led cíclicamente.
 */
static void IRAM_ATTR gpio_isr_handler(void* arg) {
    current_led = (current_led + 1) % 3;
}

/**
 * @brief Configura el botón de entrada y la interrupción asociada.
 */
static void config_button(){
    gpio_config_t io_conf = {0};
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = 1;
    
    gpio_config(&io_conf);
    gpio_set_intr_type(GPIO_INPUT_IO_0, GPIO_INTR_POSEDGE);
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void*) GPIO_INPUT_IO_0);
}

void init_rgb_leds(void) {
    setup_rgb_temp(&rgb_led_temp);
    setup_rgb_dimmer(&rgb_led_dimmer);
}

void init_button(void) {
    config_button();
}
