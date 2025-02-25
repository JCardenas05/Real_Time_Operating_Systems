#include "temp_ntc.h"

#define ADC_UNIT_NTC ADC_UNIT_2
#define CONFIG_ADC_CHANNEL_NTC ADC_CHANNEL_0

#define NTC_B 3200
#define NTC_R0 47
#define NTC_T0 298.15
#define NTC_R1 100

/*
void print_temp_callback(TimerHandle_t xTimer) {
    if (xSemaphoreTake(temp_mutex, portMAX_DELAY) == pdTRUE) {
        float current_T = last_T; // Leer última temperatura
        xSemaphoreGive(temp_mutex);

        if (temp_print_enabled) {
            char buffer[50];
            snprintf(buffer, sizeof(buffer), "Temp: %.2f°C\n", current_T);
            sendData(buffer);
        }
    }
} */

void NTC_ADC_init(ADC_Config *adc_config, NTC_Config *ntc_config, config_adc_unit adc_uint_conf){
    adc_config->channel = CONFIG_ADC_CHANNEL_NTC;
    adc_config->bitwidth = ADC_BITWIDTH_DEFAULT;
    adc_config->atten = ADC_ATTEN_DB_12;
    ntc_config->b = NTC_B;
    ntc_config->R0 = NTC_R0;
    ntc_config->T0 = NTC_T0;
    ntc_config->R1 = NTC_R1;
    adc_initialize(adc_config, adc_uint_conf);
}

static void ntc_temp_rgb_task(void *arg) {
    NTC_Config ntc_config;
    ADC_Config adc_config;

    config_adc_unit adc_uint_conf = adc_init_adc_unit(ADC_UNIT_NTC);

    NTC_ADC_init(&adc_config, &ntc_config, adc_uint_conf);

    int current_r = 0;
    int current_g = 0;
    int current_b = 0;

    int raw;
    float voltage;
    float R;
    float T;

    Thresholds_Color received_thresholds;

    while (1) {
        
        if (xSemaphoreTake(thresholds_mutex, portMAX_DELAY) == pdTRUE) {
            received_thresholds = current_thresholds;
            xSemaphoreGive(thresholds_mutex);
        }

        raw = read_adc_raw(&adc_config);
        voltage = adc_raw_to_voltage(&adc_config, raw);

        printf("Raw: %d, Voltage: %.2f V\n", raw, voltage);

        R = R_NTC(&ntc_config, voltage);
        T = T_NTC(&ntc_config, R);

        if (xSemaphoreTake(temp_mutex, portMAX_DELAY) == pdTRUE) {
            last_T = T;
            xSemaphoreGive(temp_mutex);
        }

        if (T >= received_thresholds.threshold_R[0] && T <= received_thresholds.threshold_R[1]) {
            current_r = 100;
        } else {
            current_r = 0;
        }
        if (T >= received_thresholds.threshold_G[0] && T <= received_thresholds.threshold_G[1]) {
            current_g = 100;
        } else {
            current_g = 0;
        }
        if (T >= received_thresholds.threshold_B[0] && T <= received_thresholds.threshold_B[1]) {
            current_b = 100;
        } else {
            current_b = 0;
        }
        //rgb_led_set_color(&rgb_led_temp, current_r, current_g, current_b);
        //printf("R_NTC: %.2f, T_NTC: %.2f\n", R, T);
        //vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void start_ntc_rgb_task(void){
    thresholds_mutex = xSemaphoreCreateMutex();
    temp_mutex = xSemaphoreCreateMutex();

    print_timer = xTimerCreate(
        "PrintTempTimer",
        pdMS_TO_TICKS(2000), // 2 segundos
        pdTRUE, // Auto-reload
        NULL,
        print_temp_callback
    );
    
    xTimerStart(print_timer, 0);
    
    ESP_LOGI(TAG, "STARTING WIFI APPLICATION");
	xTaskCreate(ntc_temp_rgb_task, "ntc_temp_rgb_task", 4096, NULL, configMAX_PRIORITIES - 3, NULL);
}

