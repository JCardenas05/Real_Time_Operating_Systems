#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "PWM_Control/include/pwm_control.h"
#include "driver/gpio.h"

#define EXAMPLE_ADC_ATTEN           ADC_ATTEN_DB_12
#define EXAMPLE_ADC1_CHAN0          ADC_CHANNEL_7

#define GPIO_INPUT_IO_0     0
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_0))

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

static int adc_value;
static int current_led = RED;
static RGB_Values current_values_rgb = {0, 0, 0};

static void check_button(void *arg)
{
    while (1)
    {
        if (gpio_get_level(GPIO_INPUT_IO_0)==0)
        {
            printf("Button pressed\n");
            current_led = (current_led + 1) % 3;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    xTaskCreate(check_button, "check_button", 2048, NULL, 10, NULL);


    RGB_LED rgb_led = {
        .red = { .gpio_num = 4, .channel = LEDC_CHANNEL_0 },
        .green = { .gpio_num = 17, .channel = LEDC_CHANNEL_1 },
        .blue = { .gpio_num = 5, .channel = LEDC_CHANNEL_2 },
        .config = {
            .mode = LEDC_LOW_SPEED_MODE,
            .timer = LEDC_TIMER_0,
            .duty_res = LEDC_TIMER_13_BIT,
            .frequency = 4000
        }
    };

    rgb_led_init(&rgb_led);
    printf("RGB LED configured\n");

    rgb_led_set_duty(&rgb_led, 0, 0, 0);

    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = EXAMPLE_ADC_ATTEN,
    };

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, EXAMPLE_ADC1_CHAN0, &config));

    while (1)
    {
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN0, &adc_value));
        printf("ADC%d Channel[%d] Raw: %d\n", ADC_UNIT_1 + 1, EXAMPLE_ADC1_CHAN0, adc_value);
        if (current_led == RED)
        {
            current_values_rgb.red = adc_value;
        }
        else if (current_led == GREEN)
        {
            current_values_rgb.green = adc_value;
        }
        else
        {
            current_values_rgb.blue = adc_value;
        }
        rgb_led_set_duty(&rgb_led, current_values_rgb.red, current_values_rgb.green, current_values_rgb.blue);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    //Tear Down
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
}