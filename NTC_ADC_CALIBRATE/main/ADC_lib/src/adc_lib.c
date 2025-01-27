#include "adc_lib.h"


// Función para inicializar la calibración
bool adc_calibration_init(ADC_Config *config) {
    config->calibration_handle = NULL;
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = config->unit,
        .atten = config->atten,
        .bitwidth = config->bitwidth,
    };

    esp_err_t ret = adc_cali_create_scheme_line_fitting(&cali_config, &config->calibration_handle);
    if (ret == ESP_OK) {
        ESP_LOGI("ADC_LIB", "Calibración exitosa para unidad %d, canal %d", config->unit, config->channel);
        return true;
    } else if (ret == ESP_ERR_NOT_SUPPORTED || ret == ESP_ERR_INVALID_ARG) {
        ESP_LOGW("ADC_LIB", "Calibración no soportada para esta configuración.");
    } else {
        ESP_LOGE("ADC_LIB", "Error inesperado al inicializar la calibración: %s", esp_err_to_name(ret));
    }
    return false;
}

adc_cali_handle_t adc_initialize(ADC_Config *config) {
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = config->unit,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &config->adc_handle));

    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = config->bitwidth,
        .atten = config->atten,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(config->adc_handle, config->channel, &chan_config));

    // Inicializar calibración como parte de la inicialización
    if (!adc_calibration_init(config)) {
        ESP_LOGW("ADC_LIB", "Inicialización de calibración fallida. Continuando sin calibración.");
    }

    return config->calibration_handle;
}

int read_adc_raw(ADC_Config *config) {
    int raw;
    ESP_ERROR_CHECK(adc_oneshot_read(config->adc_handle, config->channel, &raw));
    return raw;
}

float adc_raw_to_voltage(ADC_Config *config, int raw) {
    if (config->calibration_handle == NULL) {
        ESP_LOGE("ADC_LIB", "Calibración no inicializada.");
        return -1.0;
    }

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