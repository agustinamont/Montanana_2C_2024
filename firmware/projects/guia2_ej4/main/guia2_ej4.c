/*! @mainpage Proyecto de Osciloscopio
 *
 * @section genDesc Descripción General
 *
 * Este proyecto implementa un osciloscopio simple utilizando el ESP32.
 * Lee una señal analógica usando el ADC, envía los datos vía UART a una herramienta
 * de graficación serial en una PC, y genera una señal de ejemplo de ECG utilizando el DAC.
 *
 * @section hardConn Conexiones de Hardware
 *
 * |   Periférico      |   ESP32   	|
 * |:-----------------:|:--------------|
 * | Entrada Analógica (CH1) | GPIO_X 	|
 * | UART (Puerto Serial PC) | GPIO_Y	|
 *
 * @section changelog Historial de Cambios
 *
 * |   Fecha	    | Descripción                                    |
 * |:-------------:|:-----------------------------------------------|
 * | 27/09/2024    | Creación inicial del documento		           |
 *
 * @autor Agustina Montañana 
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "timer_mcu.h"
#include "uart_mcu.h"
#include "analog_io_mcu.h"
/*==================[macros and definitions]=================================*/
/*! @brief Período de refresco del sensor 1 (en microsegundos). */
#define CONFIG_SENSOR_TIMER_A 2000

/*! @brief Período de refresco del sensor 2 (en microsegundos). */
#define CONFIG_SENSOR_TIMER_B 4330

/*! @brief Base utilizada para convertir los datos a ASCII para la transmisión por UART.*/
#define BASE 10

/*! @brief Tamaño del buffer que contiene los datos de la señal ECG.*/
#define BUFFER_SIZE 231

/*==================[internal data definition]===============================*/
/*! @brief Manejador de la tarea de FreeRTOS encargada de enviar los datos leídos por el ADC a través de UART. */
TaskHandle_t send_data_task_handle = NULL;

/*! @brief Manejador de la tarea de FreeRTOS encargada de realizar la conversión del ADC.*/
TaskHandle_t adc_conversion_task_handle = NULL;

/*! @brief Manejador de la tarea de FreeRTOS encargada de la conversión DAC para generar la señal de ECG.*/ 
TaskHandle_t dac_conversion_task_handle = NULL;

/*! @brief Variable para almacenar el resultado de la conversión ADC.*/
uint16_t value = 0;

/*! @brief Contador para los datos de la señal ECG.*/
uint8_t cont_ecg = 0;

/*! @brief Buffer de datos de ECG que simula valores de una señal de ECG.*/
const char ecg[BUFFER_SIZE] = {
    76, 77, 78, 77, 79, 86, 81, 76, 84, 93, 85, 80,
    89, 95, 89, 85, 93, 98, 94, 88, 98, 105, 96, 91,
    99, 105, 101, 96, 102, 106, 101, 96, 100, 107, 101,
    94, 100, 104, 100, 91, 99, 103, 98, 91, 96, 105, 95,
    88, 95, 100, 94, 85, 93, 99, 92, 84, 91, 96, 87, 80,
    83, 92, 86, 78, 84, 89, 79, 73, 81, 83, 78, 70, 80, 82,
    79, 69, 80, 82, 81, 70, 75, 81, 77, 74, 79, 83, 82, 72,
    80, 87, 79, 76, 85, 95, 87, 81, 88, 93, 88, 84, 87, 94,
    86, 82, 85, 94, 85, 82, 85, 95, 86, 83, 92, 99, 91, 88,
    94, 98, 95, 90, 97, 105, 104, 94, 98, 114, 117, 124, 144,
    180, 210, 236, 253, 227, 171, 99, 49, 34, 29, 43, 69, 89,
    89, 90, 98, 107, 104, 98, 104, 110, 102, 98, 103, 111, 101,
    94, 103, 108, 102, 95, 97, 106, 100, 92, 101, 103, 100, 94, 98,
    103, 96, 90, 98, 103, 97, 90, 99, 104, 95, 90, 99, 104, 100, 93,
    100, 106, 101, 93, 101, 105, 103, 96, 105, 112, 105, 99, 103, 108,
    99, 96, 102, 106, 99, 90, 92, 100, 87, 80, 82, 88, 77, 69, 75, 79,
    74, 67, 71, 78, 72, 67, 73, 81, 77, 71, 75, 84, 79, 77, 77, 76, 76,
};

/*==================[internal functions declaration]=========================*/
/**
 * @fn
 * @brief Manejador de interrupción del Temporizador A. Activa las tareas de conversión ADC y envío de datos.
 * @param param No se usa.
 */
void FuncTimerA(void* param){
	vTaskNotifyGiveFromISR(adc_conversion_task_handle, pdFALSE);
	vTaskNotifyGiveFromISR(send_data_task_handle, pdFALSE);
}

/**
 * @fn
 * @brief Manejador de interrupción del Temporizador B. Activa la tarea de conversión DAC.
 * @param param No se usa.
 */
void FuncTimerB(void* param){
	//vTaskNotifyGiveFromISR(dac_conversion_task_handle, pdFALSE);
	AnalogOutputWrite(ecg[cont_ecg]);
		if(cont_ecg < (BUFFER_SIZE - 1)){
			cont_ecg++;
		}
		else{
			cont_ecg = 0;	
		}
	
}

/**
 * @fn
 * @brief Tarea de FreeRTOS que envía datos del ADC por UART.
 * @param pvParameter No se usa.
 */
static void SendData(void *pvParameter){
	while(true){
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		uint8_t *msg = UartItoa(value, BASE);
		UartSendString(UART_PC, (char*)msg);
		UartSendString(UART_PC, "\r");
	}
}

/**
 * @fn
 * @brief Tarea de FreeRTOS que realiza la conversión del ADC.
 * @param pvParameter No se usa.
 */
static void ADC_Conversion(void *pvParameter){
	while(true){
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		AnalogInputReadSingle(CH1, &value);
	}
}

/**
 * @fn
 * @brief Tarea de FreeRTOS que genera la salida DAC basada en los datos de ECG.
 * @param pvParameter No se usa.
 */
static void DAC_Conversion(void *pvParameter){
	while(true){
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		//AnalogOutputWrite(ecg[cont_ecg]);
		//if(cont_ecg < BUFFER_SIZE){
		//	cont_ecg++;
		//}
		//else{
		//	cont_ecg = 0;	
		//}
	}
}
/*==================[external functions definition]==========================*/
/**
 * @brief Punto de entrada principal de la aplicación. Inicializa los periféricos y comienza las tareas.
 */
void app_main(void){
	timer_config_t timer_sensor = {
        .timer = TIMER_A,
        .period = CONFIG_SENSOR_TIMER_A,
        .func_p = FuncTimerA,
        .param_p = NULL
    };

timer_config_t timer_sensor2 = {
        .timer = TIMER_B,
        .period = CONFIG_SENSOR_TIMER_B,
        .func_p = FuncTimerB,
        .param_p = NULL
    };

	analog_input_config_t config_ADC = {
		.input = CH1,
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

	TimerInit(&timer_sensor);
	TimerInit(&timer_sensor2);
	AnalogInputInit(&config_ADC);
	UartInit(&serial_port);
	AnalogOutputInit();
	
	xTaskCreate(&SendData, "Send Data", 2048, NULL, 4, &send_data_task_handle);
	xTaskCreate(&ADC_Conversion, "ConversionADC", 2048, NULL, 4, &adc_conversion_task_handle);
	xTaskCreate(&DAC_Conversion, "Conversion_DAC", 2048, NULL, 4, &dac_conversion_task_handle);

	TimerStart(timer_sensor.timer);
	TimerStart(timer_sensor2.timer);
}
/*==================[end of file]============================================*/