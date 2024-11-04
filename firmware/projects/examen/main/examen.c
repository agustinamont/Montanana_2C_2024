/*! @mainpage Template
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
 * | 04/11/2024 | Document creation		                         |
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
#include "buzzer.h"
#include "uart_mcu.h"
#include "analog_io_mcu.h"
/*==================[macros and definitions]=================================*/
/*! @brief Período de refresco para la lectura del sensor (en milisegundos). */
#define CONFIG_PERIOD_500 500
/*! @brief Período de refresco para el control de LEDs (en milisegundos). */
#define CONFIG_PERIOD_1000 10
/*! @brief Período de refresco para el temporizador (en milisegundos). */
#define REFRESCO_SEND_DATA 500
/*! @brief Variable auxiliar para activar o desactivar la medición y el control de LEDs. */
bool activar = true;
/*! @brief Almacena la distancia medida por el sensor en centímetros. */
float distancia = 0.00;
/*==================[internal data definition]===============================*/
/*! @brief Variable para almacenar el resultado de la conversión ADC.*/
uint16_t value1 = 0;
/*! @brief Variable para almacenar el resultado de la conversión ADC.*/
uint16_t value2 = 0;
/*! @brief Variable para almacenar el resultado de la conversión ADC.*/
uint16_t value3 = 0;

/*! @brief Handle para la tarea de sensado de distancia. */
TaskHandle_t sensar_task_handle = NULL;
/*! @brief Handle para la tarea de control de LEDs. */
TaskHandle_t leds_task_handle = NULL;
/*! @brief Manejador de la tarea de FreeRTOS encargada de enviar los datos leídos por el ADC a través de UART. */
TaskHandle_t send_data_task_handle = NULL;
/*! @brief Manejador de la tarea de FreeRTOS encargada de realizar la conversión del ADC.*/
TaskHandle_t adc_conversion_task_handle = NULL;
/*==================[internal functions declaration]=========================*/
/**
 * @fn
 * @brief Manejador de interrupción del Temporizador. Activa las tareas de conversión ADC y envío de datos.
 * @param param No se usa.
 */
void FuncTimerSendData(void* param){
	vTaskNotifyGiveFromISR(send_data_task_handle, pdFALSE);
	vTaskNotifyGiveFromISR(adc_conversion_task_handle, pdFALSE);
}

/*!
 * @brief Tarea que lee la distancia usando el sensor ultrasónico HC-SR04.
 *
 * Esta tarea lee periódicamente la distancia medida por el sensor y la almacena
 * en la variable global `distancia`.
 *
 * @param[in] pvParameter Puntero a los parámetros de la tarea.
 */
static void SensarTask(void *pvParameter){
	while(true){
		printf("Sensando\n");
		if(activar){
			distancia = HcSr04ReadDistanceInCentimeters();
		}
		vTaskDelay(CONFIG_PERIOD_500 / portTICK_PERIOD_MS);
	}
}

/*!
 * @brief Tarea que controla los LEDs según la distancia medida.
 *
 * Esta tarea enciende o apaga los LEDs dependiendo de la distancia almacenada
 * en la variable global `distancia`.
 * @param[in] pvParameter Puntero a los parámetros de la tarea.
 */
static void LedsTask(void *pvParameter){
	while(true){
		printf("Leds\n");
		if(activar){
			if(distancia > 500){
				LedOn(LED_1);       //led verde
				LedOff(LED_2);      //led amarillo
				LedOff(LED_3);      //led rojo
				GPIOOff(GPIO_0);
			}	
			else if((distancia<500) & (distancia>300)){
				LedOn(LED_1);
				LedOn(LED_2);
				LedOff(LED_3);
				GPIOOn(GPIO_0);
			}
			else if(distancia<300){
				LedOn(LED_1);
				LedOn(LED_2);
				LedOn(LED_3);
				GPIOOn(GPIO_0);
			}
		}
		else{
			LedsOffAll();
		}
		vTaskDelay(CONFIG_PERIOD_00 / portTICK_PERIOD_MS);
	}
}

/*!
 * @brief Tarea que controla el buzzer segun el estado del GPIO_0.
 *
 * Esta tarea enciende o apaga EL BUZZER dependiendo si el GPIO_0 esta en alto o en bajo.
 * Ademas, segun la distancia medida por el sensor, tiene dos delays posibles.
 *
 * @param[in] pvParameter Puntero a los parámetros de la tarea.
 */
static void BuzzerTask(void *pvParameter){
	while(true){
		if(GPIORead(GPIO_0)){
			BuzzerOn();
			if((distancia>300) & ((distancia<500))){
				vTaskDelay(CONFIG_PERIOD_1000 / portTICK_PERIOD_MS);
			}
			else if(distancia<300){
				vTaskDelay(CONFIG_PERIOD_500 / portTICK_PERIOD_MS);
			}    
		}
		vTaskDelay(CONFIG_PERIOD_500 / portTICK_PERIOD_MS);
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
		if((distancia<500) & (distancia>300)){
			UartSendString(UART_CONNECTOR, "Precaución, vehículo cerca.\r\n");
		}
		else if(distancia<300){
			UartSendString(UART_CONNECTOR, "Peligro, vehículo cerca.\r\n");
		}
	}
}

/**
 * @fn
 * @brief Tarea de FreeRTOS que realiza la conversión del ADC y compara el resultado para enviar un mensaje a traves de la aplicacion.
 * @param pvParameter No se usa.
 */
static void ADC_Conversion(void *pvParameter){
	while(true){
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		AnalogInputReadSingle(GPIO_20, &value1);
		AnalogInputReadSingle(GPIO_21, &value2);
		AnalogInputReadSingle(GPIO_22, &value3);

		//1200 equivale a 4G en mV
		if(value1+value2+value3 > 1200){
			UartSendString(UART_CONNECTOR, "Caida detectada.\r\n");
		}   
	}
}
/*==================[external functions definition]==========================*/
/**
 * @brief Punto de entrada principal de la aplicación. Inicializa los periféricos y comienza las tareas.
 */
void app_main(void){
	serial_config_t serial_port = {
		.port = UART_CONNECTOR,
		.baud_rate = 115200,
		.func_p = NULL,
		.param_p = NULL
	};
	/* Inicialización de timer send data */
    timer_config_t timer_send_data = {
        .timer = TIMER_A,
        .period = REFRESCO_SEND_DATA,
        .func_p = FuncTimerSendData,
        .param_p = NULL};

	analog_input_config_t config_ADC = {
		.input = GPIO_20,
		.mode = ADC_SINGLE,
		.func_p = NULL,
		.param_p = NULL,
		.sample_frec = 0
	};
	
	analog_input_config_t config_ADC = {
		.input = GPIO_21,
		.mode = ADC_SINGLE,
		.func_p = NULL,
		.param_p = NULL,
		.sample_frec = 0
	};

	analog_input_config_t config_ADC = {
		.input = GPIO_22,
		.mode = ADC_SINGLE,
		.func_p = NULL,
		.param_p = NULL,
		.sample_frec = 0
	};

	LedsInit();
	HcSr04Init(GPIO_3, GPIO_2);
	GPIOInit(GPIO_0, GPIO_OUTPUT);
	GPIOInit(GPIO_20, GPIO_INPUT);
	GPIOInit(GPIO_21, GPIO_INPUT);
	GPIOInit(GPIO_22, GPIO_INPUT);
	BuzzerInit(GPIO_0);
	UartInit(&serial_port);
    TimerInit(&timer_send_data);
	AnalogInputInit(&config_ADC);
	
	xTaskCreate(&SensarTask, "Sensar", 1024, NULL, 4, &sensar_task_handle);
	xTaskCreate(&LedsTask, "Leds", 1024, NULL, 4, &leds_task_handle);
	xTaskCreate(&SendData, "Send Data", 2048, NULL, 4, &send_data_task_handle);
	xTaskCreate(&ADC_Conversion, "ConversionADC", 2048, NULL, 4, &adc_conversion_task_handle);

}
/*==================[end of file]============================================*/