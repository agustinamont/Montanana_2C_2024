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
#include "timer_mcu.h"
/*==================[macros and definitions]=================================*/
/*! @brief Período de refresco para la lectura del sensor y el control de alarma de precaucion (en milisegundos). */
#define CONFIG_PERIOD_500 500
/*! @brief Período de refresco para el control de alarma de peligro (en milisegundos). */
#define CONFIG_PERIOD_250 250
/*! @brief Período de refresco para el control de LEDs (en milisegundos). */
#define CONFIG_PERIOD_100 100
/*! @brief Período de refresco para el muestreo de conversión ADC (en microsegundos). */
#define REFRESCO_CONVERSION_ADC 10000
/*! @brief Variable auxiliar para activar o desactivar la medición y el control de LEDs. */
bool activar = true;
/*! @brief Almacena la distancia medida por el sensor en centímetros. */
float distancia = 0.00;
/*==================[internal data definition]===============================*/
/*! @brief Variable para almacenar el resultado de la conversión ADC.*/
uint16_t senial_aceleracion_x = 0;
/*! @brief Variable para almacenar el resultado de la conversión ADC.*/
uint16_t senial_aceleracion_y = 0;
/*! @brief Variable para almacenar el resultado de la conversión ADC.*/
uint16_t senial_aceleracion_z = 0;

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
 * @brief Manejador de interrupción del Temporizador. Activa la tarea de conversión ADC.
 * @param param No se usa.
 */
void FuncTimerConversionADC(void* param){
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
 * @brief Tarea que controla las alarmas según la distancia medida.
 *
 * Esta tarea enciende o apaga los LEDs y el buzzer dependiendo de la distancia almacenada
 * en la variable global `distancia`. Tambien envia el mensaje correspondiente a la UART.
 * @param[in] pvParameter Puntero a los parámetros de la tarea.
 */
static void AlarmasTask(void *pvParameter){
	while(true){
		printf("Leds\n");
		if(activar){
			if(distancia > 500){
				LedOn(LED_1);       //led verde
				LedOff(LED_2);      //led amarillo
				LedOff(LED_3);      //led rojo
				GPIOOff(GPIO_0);
			}	
			else if((distancia<500) && (distancia>300)){
				LedOn(LED_1);
				LedOn(LED_2);
				LedOff(LED_3);
				
				GPIOOn(GPIO_0);
				vTaskDelay(CONFIG_PERIOD_500 / portTICK_PERIOD_MS);
				GPIOOff(GPIO_0); 
				vTaskDelay(CONFIG_PERIOD_500 / portTICK_PERIOD_MS);

				UartSendString(UART_CONNECTOR, "Precaución, vehículo cerca.\r\n");
			}
			else if(distancia<300){
				LedOn(LED_1);
				LedOn(LED_2);
				LedOn(LED_3);
				
				GPIOOn(GPIO_0);
				vTaskDelay(CONFIG_PERIOD_250 / portTICK_PERIOD_MS);
				GPIOOff(GPIO_0); 
				vTaskDelay(CONFIG_PERIOD_250 / portTICK_PERIOD_MS);

				UartSendString(UART_CONNECTOR, "Peligro, vehículo cerca.\r\n");
			}
		}
		else{
			LedsOffAll();
		}
		vTaskDelay(CONFIG_PERIOD_100 / portTICK_PERIOD_MS);
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
		AnalogInputReadSingle(CH1, &senial_aceleracion_x);
		AnalogInputReadSingle(CH2, &senial_aceleracion_y);
		AnalogInputReadSingle(CH3, &senial_aceleracion_z);

		float aceleracion_x = (senial_aceleracion_x / 1000.0 - 1.65) / 0.3;
		float aceleracion_y = (senial_aceleracion_y / 1000.0 - 1.65) / 0.3;
		float aceleracion_z = (senial_aceleracion_z / 1000.0 - 1.65) / 0.3;

		if ((aceleracion_x + aceleracion_y + aceleracion_z) > 4){
			UartSendString(UART_CONNECTOR, "Caida detectada\r\n");
		}   
	}
}
/*==================[external functions definition]==========================*/
/**
 * @brief Punto de entrada principal de la aplicación. Inicializa los periféricos y comienza las tareas.
 */
void app_main(void){
	LedsInit();
	HcSr04Init(GPIO_3, GPIO_2);
	GPIOInit(GPIO_0, GPIO_OUTPUT);

	serial_config_t serial_port = {
		.port = UART_CONNECTOR,
		.baud_rate = 115200,
		.func_p = NULL,
		.param_p = NULL
	};

	/* Inicialización de timer conversion_ADC */
    timer_config_t timer_conversion_adc = {
        .timer = TIMER_A,
        .period = CONFIG_PERIOD_500,
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

	analog_input_config_t config_ADC_CH3 = {
		.input = CH3,
		.mode = ADC_SINGLE,
		.func_p = NULL,
		.param_p = NULL,
		.sample_frec = 0
	};

	UartInit(&serial_port);
    TimerInit(&timer_conversion_adc);
	AnalogInputInit(&config_ADC_CH1);
	AnalogInputInit(&config_ADC_CH2);
	AnalogInputInit(&config_ADC_CH3);

	
	xTaskCreate(&SensarTask, "Sensar", 1024, NULL, 4, &sensar_task_handle);
	xTaskCreate(&AlarmasTask, "Leds", 1024, NULL, 3, &leds_task_handle);
	xTaskCreate(&ADC_Conversion, "ConversionADC", 2048, NULL, 2, &adc_conversion_task_handle);

}
/*==================[end of file]============================================*/