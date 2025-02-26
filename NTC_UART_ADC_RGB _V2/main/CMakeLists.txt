idf_component_register(SRCS "peripherals.c" "main.c" "ADC_lib/src/adc_lib.c" "NTC_lib/src/ntc_lib.c" "PWM_Control/src/pwm_control.c"
                    INCLUDE_DIRS "." "ADC_lib/include" "NTC_lib/include" "PWM_Control/include")
