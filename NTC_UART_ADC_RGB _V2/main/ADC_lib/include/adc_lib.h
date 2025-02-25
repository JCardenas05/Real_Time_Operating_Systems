//adc_lib.h ----------------------
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

typedef struct ADC_Config {
    adc_unit_t unit;
    adc_channel_t channel;
    adc_bitwidth_t bitwidth;
    adc_atten_t atten;
    adc_cali_handle_t calibration_handle; // Manejador de calibración
    adc_oneshot_unit_handle_t adc_handle; // Manejador del ADC
} ADC_Config;

typedef struct config_adc_unit {
    adc_unit_t unit;
    adc_oneshot_unit_handle_t adc_handle;
} config_adc_unit;

config_adc_unit adc_init_adc_unit(adc_unit_t unit);
bool adc_calibration_init(ADC_Config *config);
adc_cali_handle_t adc_initialize(ADC_Config *config, config_adc_unit unit_conf);
int read_adc_raw(ADC_Config *config);
float adc_raw_to_voltage(ADC_Config *config, int raw);
void adc_calibration_deinit(ADC_Config *config);