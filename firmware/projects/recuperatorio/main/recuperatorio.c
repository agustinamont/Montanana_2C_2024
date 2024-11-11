/*! @mainpage Recuperatorio
 *
 * @section genDesc General Description
 *
 * This section describes how the program works.
 *
 * <a href="https://drive.google.com/...">Operation Example</a>
 *
 * @section hardConn Hardware Connection
 *
 * |    Peripheral  |   ESP32   	|
 * |:--------------:|:--------------|
 * | 	PIN_X	 	| 	GPIO_X		|
 *
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 11/11/2024 | Document creation		                         |
 *
 * @author Agustina Montañana (agustina.montanana@ingenieria.uner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
#include "hc_sr04.h"
#include "gpio_mcu.h"
#include "uart_mcu.h"
#include "analog_io_mcu.h"
#include "timer_mcu.h"
/*==================[macros and definitions]=================================*/
#define CONFIG_PERIOD_100 100 //tiempo de medicion del sensor en milisegundos (10 muestras x seg)
#define REFRESCO_TIMER 5000 //200 muestras por segundo son equivale a 1 muestra cada 5000 microsegundos
bool activar = true;
float distancia = 0.00;
float tiempo = 0.10;   //tiempo entre la toma de cada muestra (en segundos)
float prom_peso_galga_1 = 0.00;
float prom_peso_galga_2 = 0.00;
float peso_total = 0.00;
/*==================[internal data definition]===============================*/
uint16_t valor_galga_1 = 0;
uint16_t valor_galga_2 = 0;
uint8_t cont_muestras = 0;

TaskHandle_t medir_task_handle = NULL;
TaskHandle_t leds_task_handle = NULL;
TaskHandle_t adc_conversion_task_handle = NULL;
/*==================[internal functions declaration]=========================*/
void FuncTimerConversionADC(void* param){
	vTaskNotifyGiveFromISR(adc_conversion_task_handle, pdFALSE);
}

static void MedirTask(void *pvParameter){
	while(true){
		printf("Sensando\n");
		if(activar){
			distancia = HcSr04ReadDistanceInCentimeters();
		}
		vTaskDelay(CONFIG_PERIOD_100 / portTICK_PERIOD_MS);
	}
}

static void LedsTask(void *pvParameter){
	while(true){
		printf("Leds\n");
		if(activar){
			if(distancia < 1000){
				float velocidad = (distancia/tiempo);
				
				UartSendString(UART_PC, "Velocidad maxima: ");
				UartSendString(UART_PC, (char *)UartItoa(velocidad, 10));
				UartSendString(UART_PC, " m/s\r\n");

				if(velocidad > 800){
					LedOff(LED_1);
					LedOff(LED_2);
					LedOn(LED_3);
				}
				else if((velocidad < 800) && (velocidad > 0)){
					LedOff(LED_1);
					LedOn(LED_2);
					LedOff(LED_3);
				}
				else if(velocidad == 0){
					LedOn(LED_1);
					LedOff(LED_2);
					LedOff(LED_3);

					if(cont_muestras < 50){
						float peso_galga1 = ((valor_galga_1 * 20000) / 3300);
						float peso_galga2 = ((valor_galga_2 * 20000) / 3300);

						prom_peso_galga_1 =+ peso_galga1/cont_muestras;
						prom_peso_galga_2 =+ peso_galga2/cont_muestras;
						cont_muestras++;
					}
					peso_total = prom_peso_galga_1 + prom_peso_galga_2;
		
					UartSendString(UART_PC, "Peso: ");
					UartSendString(UART_PC,  (char *)UartItoa(peso_total, 10));
					UartSendString(UART_PC, " kg\r\n");
				}
			}	
			else{
				LedsOffAll();
			}
		}
		vTaskDelay(CONFIG_PERIOD_100 / portTICK_PERIOD_MS);
	}
}

static void ADC_Conversion(void *pvParameter){
	while(true){
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		AnalogInputReadSingle(CH1, &valor_galga_1);
		AnalogInputReadSingle(CH2, &valor_galga_2);
	}
}

void ControlBarrera(){
    uint8_t tecla;
    UartReadByte(UART_PC, &tecla);
    switch (tecla){
    case 'O':
        GPIOOn(GPIO_1);
        break;
	case 'C':
		GPIOOff(GPIO_1);
		break;
	}
}
/*==================[external functions definition]==========================*/
void app_main(void){
	HcSr04Init(GPIO_3, GPIO_2);
	LedsInit();
	GPIOInit(GPIO_1, GPIO_OUTPUT);
	
	/* Inicialización de timer conversion_ADC */
    timer_config_t timer_conversion_adc = {
        .timer = TIMER_A,
        .period = REFRESCO_TIMER,
        .func_p = FuncTimerConversionADC,
        .param_p = NULL
	};

	analog_input_config_t config_ADC_CH1 = {
		.input = CH1,
		.mode = ADC_SINGLE,
		.func_p = NULL,
		.param_p = NULL,
		.sample_frec = 0
	};
	
	analog_input_config_t config_ADC_CH2 = {
		.input = CH2,
		.mode = ADC_SINGLE,
		.func_p = NULL,
		.param_p = NULL,
		.sample_frec = 0
	};

	serial_config_t serial_port = {
		.port = UART_PC,
		.baud_rate = 115200,
		.func_p = NULL,
		.param_p = NULL
	};

	TimerInit(&timer_conversion_adc);
	AnalogInputInit(&config_ADC_CH1);
	AnalogInputInit(&config_ADC_CH2);
	UartInit(&serial_port);

	xTaskCreate(&MedirTask, "Medir", 1024, NULL, 4, &medir_task_handle);
	xTaskCreate(&LedsTask, "Leds", 1024, NULL, 3, &leds_task_handle);
	xTaskCreate(&ADC_Conversion, "ConversionADC", 2048, NULL, 2, &adc_conversion_task_handle);

}
/*==================[end of file]============================================*/