#ifndef RGB_LED_H
#define RGB_LED_H

#include "driver/ledc.h"
#include "esp_err.h"

// PWM configuration structure
typedef struct PWM_Config {
    ledc_mode_t mode;
    ledc_timer_t timer;
    ledc_timer_bit_t duty_res;
    uint32_t frequency;
} PWM_Config;

// GPIO-PWM channel structure
typedef struct PWM_Channel {
    uint8_t gpio_num;
    ledc_channel_t channel;
} PWM_Channel;

// RGB LED configuration structure
typedef struct RGB_LED {
    PWM_Channel red;
    PWM_Channel green;
    PWM_Channel blue;
    PWM_Config config;
} RGB_LED;

// Functions
void pwm_channel_init(const PWM_Config *config, const PWM_Channel *channel);
void pwm_set_duty(const PWM_Channel *channel, uint32_t duty, ledc_mode_t mode);
void rgb_led_init(const RGB_LED *rgb_led);
void rgb_led_set_duty(const RGB_LED *rgb_led, uint32_t red_duty, uint32_t green_duty, uint32_t blue_duty);

#endif // RGB_LED_H
