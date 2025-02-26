#include "math.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

typedef struct NTC_Config {
    float b; // B value of the NTC thermistor
    float R0; // R0 value of the NTC thermistor
    float T0; // T0 value of the NTC thermistor
    float R1; // R1 value of the tension divider resistor
} NTC_Config;

/**
 * @brief Calculate the resistance of the NTC thermistor
 * @param config Pointer to the NTC_Config structure
 * @param V_NTC Voltage read from the NTC thermistor
 */
float R_NTC(NTC_Config *config, float V_NTC);

/**
 * @brief Calculate the temperature of the NTC thermistor
 * @param config Pointer to the NTC_Config structure
 * @param R_NTC Resistance of the NTC thermistor
 */

float T_NTC(NTC_Config *config, float R_NTC);

