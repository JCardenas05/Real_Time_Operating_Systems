#include "PWM_Control/include/pwm_control.h"
#include "ADC_lib/include/adc_lib.h"
#include "freertos/FreeRTOS.h"

typedef struct RGB_Values
{
    int red;
    int green;
    int blue;
} RGB_Values;

enum current_led
{
    RED,
    GREEN,
    BLUE
};

extern int current_led = RED;
static const char TAG [] = "DIMMER_TASK";