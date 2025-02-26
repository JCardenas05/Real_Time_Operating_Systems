#include "ntc_lib.h"

float R_NTC(NTC_Config *config, float V_NTC) {
    return (config->R1 * V_NTC) / (3.3 - V_NTC);
}

float T_NTC(NTC_Config *config, float R_NTC) {
    float T_K = 1/((1/(config->T0))+(log(R_NTC/config->R0)/config->b));
    return T_K - 273.15;
}