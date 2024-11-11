/*! @mainpage Recuperatorio
 *
 * @section genDesc General Description
 *
 * Esta aplicación implementa un sistema de pesaje de camiones basado en la placa ESP-EDU.
 * A través del sensor de ultrasonido HC-SR04 se mide la distancia del camión para ingresar a la balanza.
 * Se calcula la velocidad y según su valor se encienden señales de advertencia.
 * Cuando el vehiculo se detiene, se procede a pesarlo y se obtienen los valores a través de dos entradas 
 * analógicas (son dos balanzas). Se informa a la PC la velocidad y el peso del camión, a través de la UART
 * y se maneja desde la PC  (también a través de la UART) el control de una barrera.
 *
 * <a href="https://drive.google.com/...">Operation Example</a>
 *
 * @section hardConn Hardware Connection
 *
 * |    HC-SR04     |   ESP32   	|
 * |:--------------:|:--------------|
 * | 	Vcc 	    |	5V      	|
 * | 	Echo		| 	GPIO_3		|
 * | 	Trig	 	| 	GPIO_2		|
 * | 	Gnd 	    | 	GND     	|
 * 
 * |    UART_PC     |   ESP32   	|
 * |:--------------:|:--------------|
 * | 	  TX        | 	GPIO_16		|
 * | 	  RX        | 	GPIO_17		|
 * | 	  Gnd       | 	GND    	    |
 * 
 * |    Barrera     |   ESP32   	|
 * |:--------------:|:--------------|
 * | 	Señal		| 	GPIO_1		|
 * | 	Gnd 	    | 	GND     	|
 *
 * |    Balanzas    |   ESP32   	|
 * |:--------------:|:--------------|
 * | 	Balanza 1	| 	CH1 		|
 * | 	Balanza 2	| 	CH2 		|

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
/*! @brief Período de refresco para la lectura del sensor (en milisegundos, equivale a 10 muestras por segundo). */
#define CONFIG_PERIOD_100 100
/*! @brief Período de refresco para el temporizador (en microsegundos, equivale a 200 muestras por segundo). */
#define REFRESCO_TIMER 5000
/*! @brief Variable auxiliar para activar o desactivar la medición y el control de LEDs. */
bool activar = true;
/*! @brief Almacena el tiempo entra la toma de cada muestra (en segundos).*/
float tiempo = 0.10;
 /*! @brief Almacena la velocidad maxima calculada.*/
 float velocidad_max = 0.00;
/*! @brief Almacena el promedio de las mediciones de la balanza 1.*/
float prom_peso_galga_1 = 0.00;
/*! @brief Almacena el promedio de las mediciones de la balanza 2.*/
float prom_peso_galga_2 = 0.00;
/*! @brief Almacena la suma del promedio de ambas balanzas.*/
float peso_total = 0.00;
/*==================[internal data definition]===============================*/
/*! @brief Variable para almacenar la medicion actual del sensor HC-SR04.*/
uint16_t distancia_actual = 0;
/*! @brief Variable para almacenar la medicion anterior del sensor HC-SR04.*/
uint16_t distancia_anterior = 100;
/*! @brief Variable para almacenar el resultado de la conversión ADC.*/
uint16_t valor_galga_1 = 0;
/*! @brief Variable para almacenar el resultado de la conversión ADC.*/
uint16_t valor_galga_2 = 0;
/*! @brief Variable auxiliar para contar la cantidad de muestras a promediar.*/
uint8_t cont_muestras = 0;

/*! @brief Handle para la tarea de medición de distancia. */
TaskHandle_t medir_task_handle = NULL;
/*! @brief Handle para la tarea de control de LEDs. */
TaskHandle_t leds_task_handle = NULL;
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
 * en la variable global `distancia_actual`.
 *
 * @param[in] pvParameter Puntero a los parámetros de la tarea.
 */
static void MedirTask(void *pvParameter){
	while(true){
		printf("Sensando\n");
		if(activar){
			distancia_actual = HcSr04ReadDistanceInCentimeters();
		}
		vTaskDelay(CONFIG_PERIOD_100 / portTICK_PERIOD_MS);
	}
}

/*!
 * @brief Tarea que controla las advertencias de velocidad según la distancia medida.
 *
 * Esta tarea enciende o apaga un LED dependiendo de la velocidad calculada. Tambien 
 * envia el mensaje correspondiente a la UART y calcula el peso del vehiculo.
 * @param[in] pvParameter Puntero a los parámetros de la tarea.
 */
static void LedsTask(void *pvParameter){
	while(true){
		printf("Leds\n");
		if(activar){
			if(distancia_actual < 1000){
				float velocidad = ((distancia_anterior - distancia_actual)/tiempo);
				
				if(velocidad>velocidad_max){
					velocidad_max = velocidad;
				}

				UartSendString(UART_PC, "Velocidad maxima: ");
				UartSendString(UART_PC, (char *)UartItoa(velocidad_max, 10));
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
		distancia_anterior = distancia_actual;
		vTaskDelay(CONFIG_PERIOD_100 / portTICK_PERIOD_MS);
	}
}

/**
 * @fn
 * @brief Tarea de FreeRTOS que realiza la conversión del ADC y compara el 
 * resultado para enviar un mensaje a traves de la aplicacion.
 * @param pvParameter No se usa.
 */
static void ADC_Conversion(void *pvParameter){
	while(true){
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		AnalogInputReadSingle(CH1, &valor_galga_1);
		AnalogInputReadSingle(CH2, &valor_galga_2);
	}
}

/**
 * @brief Función de interrupción para los comandos enviados desde el puerto serie
 * 
 * Esta función lee los comandos 'O' y 'C' desde el puerto serie, y abre o cierra la 
 * barrera a través del GPIO1.
 */
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
/**
 * @brief Punto de entrada principal de la aplicación. Inicializa los periféricos y comienza las tareas.
 */
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