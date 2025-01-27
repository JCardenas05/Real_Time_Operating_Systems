#include "adc_lib.h"

adc_cali_handle_t adc_initialize(ADC_Config *config) {
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = config->unit,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = config->bitwidth,
        .atten = config->atten,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, config->channel, &chan_config));

    // Calibración específica para este ADC
    config->calibration_handle = NULL;
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = config->unit,
        .atten = config->atten,
        .bitwidth = config->bitwidth,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_line_fitting(&cali_config, &config->calibration_handle));

    return config->calibration_handle;
}

int read_adc_raw(ADC_Config *config) {
    int raw;
    ESP_ERROR_CHECK(adc_oneshot_read(config->, config->channel, &raw));
    return raw;
}

float adc_raw_to_voltage(ADC_Config *config, int raw) {
    int voltage_mv;
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(config->calibration_handle, raw, &voltage_mv));
    return voltage_mv / 1000.0;
}

void adc_calibration_deinit(ADC_Config *config) {
    if (config->calibration_handle) {
        ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(config->calibration_handle));
        config->calibration_handle = NULL;
    }
}
