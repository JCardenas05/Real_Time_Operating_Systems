idf_component_register(SRCS "main.c" "wifi_manager/src/wifi_manager.c" "server/src/server.c" "peripherals.c" "ADC_lib/src/adc_lib.c" "NTC_lib/src/ntc_lib.c" "PWM_Control/src/pwm_control.c"
                    INCLUDE_DIRS "." "wifi_manager/include" "server/include" "ADC_lib/include" "NTC_lib/include" "PWM_Control/include"
                    EMBED_FILES webpage/styles.css webpage/scripts.js webpage/index.html webpage/credenciales.html webpage/lectura_pot.html webpage/led_rgb.html webpage/lectura_temp.html)
