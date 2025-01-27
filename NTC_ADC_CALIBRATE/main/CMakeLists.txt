idf_component_register(SRCS "oneshot_read_main.c" "NTC_lib/src/ntc_lib.c" "ADC_lib/src/adc_lib.c"
                    INCLUDE_DIRS "." NTC_lib/include ADC_lib/include)
