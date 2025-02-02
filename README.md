# RTOS (Real_Time_Operating_Systems)

- Nicolas Botero Diaz
- Jony Cardenas Herrera

## Archivos
- [**`led_button_input`**](led_button_input/): Controla el blink de un LED utilizando un botón de entrada con tareas.
- [**`PWM_Control`**](PWM_Control/): Biblioteca para controlar salidas PWM, permitiendo el control de LED RGB con funcionalidades para mapear valores entre 0 y 100, y establecer colores usando una estructura (R,G,B).
- [**`RGB_Control`**](RGB_Control/): Proyecto de ejemplo de aplicación de la biblioteca PWM_Control para controlar un LED RGB.
- [**`ADC_minimum_configuration`**](ADC_minimum_configuration/): Proyecto de ejemplo de configuración mínima para la lectura de un ADC.
- [**`ADC_PWM_DI`**](ADC_PWM_DI/): Proyecto que integra ADC, PWM y DI Entradas Digitales para controlar un LED RGB. Utiliza un potenciómetro para ajustar los colores individuales (R,G,B) del LED y un botón para alternar entre modos de control, permitiendo manejar todos los canales de color de forma individual mediante el mismo potenciómetro.
- [**`NTC_UART_ADC_RGB`**](NTC_UART_ADC_RGB/): Proyecto desarrollado para el primer parcial, cuyo objetivo fue controlar el color de un LED RGB en función de la temperatura medida por una termistancia NTC. El sistema permite, a través de comandos UART, establecer los rangos de temperatura que corresponden a cada color del LED, así como iniciar o detener el muestreo de la temperatura mediante la consola UART.
