#include "PWM_Control/include/pwm_control.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void) {
    // Define RGB LED configuration
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

    // Initialize RGB LED
    rgb_led_init(&rgb_led);
    printf("RGB LED configured\n");

    while (1) {
        for (int i = 0; i <= 100; i += 1) {
            rgb_led_set_color(&rgb_led, i, 0, 0); // Fade red and green up
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }

        for (int i = 0; i <= 100; i += 1) {
            rgb_led_set_color(&rgb_led, 100-i, i, 0); // Fade green up
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }

        for (int i = 0; i <= 100; i += 1) {
            rgb_led_set_color(&rgb_led, 0, 100-i, i); // Fade blue up
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }

        rgb_led_set_color(&rgb_led, 0, 0, 0); 
        vTaskDelay(100 / portTICK_PERIOD_MS);

    }
}
