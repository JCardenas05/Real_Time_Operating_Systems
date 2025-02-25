#include "ADC_lib/include/adc_lib.h"
#include "NTC_lib/include/ntc_lib.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <stdio.h>
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char TAG [] = "NTC_RGB";
static volatile float last_T = 0.0;

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

static SemaphoreHandle_t thresholds_mutex;
static SemaphoreHandle_t temp_mutex;
extern volatile bool temp_print_enabled = true; 
static TimerHandle_t print_timer; 
