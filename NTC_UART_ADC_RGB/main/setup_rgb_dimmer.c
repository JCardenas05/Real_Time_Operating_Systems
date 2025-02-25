#include "setup_rgb_dimmer.h"

#define GPIO_R_DIMMER 12
#define GPIO_G_DIMMER 27
#define GPIO_B_DIMMER 26

#define CONFIG_ADC_CHANNEL_DIMMER ADC_CHANNEL_5
#define ADC_UNIT_DIMMER ADC_UNIT_1

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

void dimmer_RGB_task(void *arg) {
    RGB_LED rgb_led_dimmer;
    setup_rgb_dimmer(&rgb_led_dimmer);
    rgb_led_set_color(&rgb_led_dimmer, 0, 0, 0);

    config_adc_unit adc_uint_conf_dimmer = adc_init_adc_unit(ADC_UNIT_DIMMER);

    ADC_Config adc_config_dimmer;
    adc_config_dimmer.channel = CONFIG_ADC_CHANNEL_DIMMER;
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
        }else
        {
            current_values_rgb.blue = raw_dimmer;
        }
        rgb_led_set_duty(&rgb_led_dimmer, current_values_rgb.red, current_values_rgb.green, current_values_rgb.blue);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void star_dimmer_task(void){
    ESP_LOGI(TAG, "STARTING WIFI APPLICATION");
    xTaskCreate(dimmer_RGB_task, "dimmer_rbg_task", 4096, NULL, configMAX_PRIORITIES - 4, NULL);
}