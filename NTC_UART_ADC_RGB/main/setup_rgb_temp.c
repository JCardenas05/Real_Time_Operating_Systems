#include "setup_rgb_temp.h"

#define GPIO_R_TEMP 21
#define GPIO_G_TEMP 19
#define GPIO_B_TEMP 18

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
    rgb_led_temp->config.invert = 0;

    rgb_led_init(rgb_led_temp);
    printf("RGB LED Temp configured\n");

}

void start_rgb_temp_task(void){
    RGB_LED rgb_led_temp;
    setup_rgb_temp(&rgb_led_temp);
    rgb_led_set_duty(&rgb_led_temp, 0, 0, 0);

    xTaskCreate(setup_rgb_temp, "RGB_task_temp", 1024 * 2, NULL, configMAX_PRIORITIES - 1, NULL);
}

