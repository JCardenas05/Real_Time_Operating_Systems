#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include "PWM_Control/include/pwm_control.h"
#include "ADC_lib/include/adc_lib.h"
#include "NTC_lib/include/ntc_lib.h"

// Configuraciones de ADC
#define CONFIG_ADC_CHANNEL_NTC      ADC_CHANNEL_0
#define CONFIG_ADC_CHANNEL_DIMMER   ADC_CHANNEL_5

#define ADC_UNIT_NTC                ADC_UNIT_2
#define ADC_UNIT_DIMMER             ADC_UNIT_1

// Configuraciones del botón
#define GPIO_INPUT_IO_0             0
#define GPIO_INPUT_PIN_SEL          ((1ULL << GPIO_INPUT_IO_0))
#define ESP_INTR_FLAG_DEFAULT       0

// Configuraciones para RGB LED de temperatura
#define GPIO_R_TEMP                 21
#define GPIO_G_TEMP                 19
#define GPIO_B_TEMP                 18

// Configuraciones para RGB LED del dimmer
#define GPIO_R_DIMMER               12
#define GPIO_G_DIMMER               27
#define GPIO_B_DIMMER               26

// Configuraciones para NTC
#define NTC_B                       3200
#define NTC_R0                      47
#define NTC_T0                      298.15
#define NTC_R1                      100

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Variable externa que indica el LED actual.
 */
extern int current_led;

/**
 * @brief Inicializa la configuración del ADC y parámetros del NTC.
 *
 * @param adc_config Puntero a la configuración del ADC.
 * @param ntc_config Puntero a la configuración del NTC.
 * @param adc_uint_conf Unidad ADC a utilizar.
 */
void NTC_ADC_init(ADC_Config *adc_config, NTC_Config *ntc_config, config_adc_unit adc_uint_conf);

/**
 * @brief Inicializa los dos RGB LEDs (temperatura y dimmer).
 *
 * Esta función configura internamente los parámetros de cada LED y llama a
 * rgb_led_init() (definida en la librería PWM_Control).
 */
void init_rgb_leds(void);

/**
 * @brief Inicializa el botón de entrada y configura su interrupción.
 */
void init_button(void);

/**
 * @brief Configura el RGB LED para temperatura.
 *
 * Se asignan los pines, canales y parámetros de configuración.
 */
void setup_rgb_temp(RGB_LED *rgb_led_temp);

/**
 * @brief Configura el RGB LED para el dimmer.
 *
 * Se asignan los pines, canales y parámetros de configuración.
 */
void setup_rgb_dimmer(RGB_LED *rgb_led_dimmer);

#ifdef __cplusplus
}
#endif

#endif // PERIPHERALS_H
