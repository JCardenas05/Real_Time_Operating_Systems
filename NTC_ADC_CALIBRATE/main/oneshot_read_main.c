#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "math.h"
#include "NTC_lib/include/ntc_lib.h"
#include "ADC_lib/include/adc_lib.h"

#define EXAMPLE_ADC1_CHAN0          ADC_CHANNEL_4
#define EXAMPLE_ADC_ATTEN           ADC_ATTEN_DB_12


void app_main(void)
{
    ADC_Config adc_config = {
        .unit = ADC_UNIT_1,
        .channel = ADC_CHANNEL_4,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12
    };

    float T;
    float R;

    NTC_Config ntc_config = {
        .b = 3200,
        .R0 = 47,
        .T0 = 298.15,
        .R1 = 100
    };

    adc_initialize(&adc_config);

    while (1) {
        int raw = read_adc_raw(&adc_config);
        float voltage = adc_raw_to_voltage(&adc_config, raw);

        printf("Raw: %d, Voltage: %.2f V\n", raw, voltage);
        R = R_NTC(&ntc_config, voltage);
        T = T_NTC(&ntc_config, R);

        printf("R_NTC: %.2f, T_NTC: %.2f\n", R, T);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    /*

    adc_calibration_deinit(&adc_config);


    float T;
    float V;
    float R;

    NTC_Config ntc_config = {
        .b = 3200,
        .R0 = 47,
        .T0 = 298.15,
        .R1 = 150
    };

    ESP_LOGI(TAG, "T_25C_Check: %.2f", T_NTC(&ntc_config, 47));

    while (1) {
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN0, &adc_raw));
        ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, EXAMPLE_ADC1_CHAN0, adc_raw);
        if (do_calibration1_chan0) {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_chan0_handle, adc_raw, &voltage));
            ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, EXAMPLE_ADC1_CHAN0, voltage);
            V = voltage/1000.0;
            R = R_NTC(&ntc_config, V);
            T = T_NTC(&ntc_config, R);

            ESP_LOGI(TAG, "R_NTC: %.2f", R);
            ESP_LOGI(TAG, "T_NTC: %.2f", T);

        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    //Tear Down
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
    if (do_calibration1_chan0) {
        example_adc_calibration_deinit(adc1_cali_chan0_handle);
    }
    */
}
