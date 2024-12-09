#include "rgb_led.h"

void pwm_channel_init(const PWM_Config *config, const PWM_Channel *channel) {
    // Configure LEDC timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = config->mode,
        .duty_resolution  = config->duty_res,
        .timer_num        = config->timer,
        .freq_hz          = config->frequency,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Configure LEDC channel
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = config->mode,
        .channel        = channel->channel,
        .timer_sel      = config->timer,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = channel->gpio_num,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void pwm_set_duty(const PWM_Channel *channel, uint32_t duty, ledc_mode_t mode) {
    ledc_set_duty(mode, channel->channel, duty);
    ledc_update_duty(mode, channel->channel);
}

void rgb_led_init(const RGB_LED *rgb_led) {
    pwm_channel_init(&rgb_led->config, &rgb_led->red);
    pwm_channel_init(&rgb_led->config, &rgb_led->green);
    pwm_channel_init(&rgb_led->config, &rgb_led->blue);
}

void rgb_led_set_duty(const RGB_LED *rgb_led, uint32_t red_duty, uint32_t green_duty, uint32_t blue_duty) {
    pwm_set_duty(&rgb_led->red, red_duty, rgb_led->config.mode);
    pwm_set_duty(&rgb_led->green, green_duty, rgb_led->config.mode);
    pwm_set_duty(&rgb_led->blue, blue_duty, rgb_led->config.mode);
}
