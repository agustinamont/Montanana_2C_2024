/*! @mainpage Proyecto: Medidor de Distancia por Ultrasonido
 *
 * @section genDesc Descripción General
 *
 * Este firmware está diseñado para funcionar con un sensor de distancia ultrasónico (HC-SR04), LEDs y un display LCD para medir y mostrar distancias. Controla un conjunto de LEDs en función de la distancia medida y utiliza las teclas (TEC1 y TEC2) para la interacción con el usuario.
 * 
 * - TEC1 activa o detiene la medición de distancia.
 * - TEC2 mantiene el último valor de distancia medido en el LCD y congela el estado de los LEDs.
 * - La distancia medida se actualiza cada 1 segundo.
 *
 * @section funcDesc Descripción Funcional
 *
 * Los LEDs operan de acuerdo a la siguiente lógica:
 * - Distancia < 10 cm: Apagar todos los LEDs.
 * - 10 cm <= Distancia < 20 cm: Encender LED_1.
 * - 20 cm <= Distancia < 30 cm: Encender LED_1 y LED_2.
 * - Distancia >= 30 cm: Encender LED_1, LED_2 y LED_3.
 *
 * La distancia actual también se muestra en el display LCD en centímetros.
 *
 * @section hardConn Conexiones de Hardware
 *
 * |   Periférico   |  Pin ESP32   	|
 * |:--------------:|:--------------|
 * | HC-SR04 Echo   | GPIO_X        |
 * | HC-SR04 Trigger| GPIO_X        |
 * | LED_1          | GPIO_X        |
 * | LED_2          | GPIO_X        |
 * | LED_3          | GPIO_X        |
 * | LCD Display    | GPIO_X        |
 * | TEC1           | GPIO_X        |
 * | TEC2           | GPIO_X        |
 *
 * @section changelog Historial de Cambios
 *
 * |    Fecha      | Descripción                                    |
 * |:-------------:|:-----------------------------------------------|
 * | 06/09/2024    | Creación de la documentación                   |
 *
 * @section author Autor
 * - Agustina Montañana (agustina.montanana@ingenieria.uner.edu.ar)
 */
/*==================[inclusiones]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
#include "hc_sr04.h"
#include "lcditse0803.h"
#include "gpio_mcu.h"
#include "switch.h"

/*==================[macros and definitions]===================================*/
/*! @brief Período de refresco para la lectura del sensor (en milisegundos). */
#define CONFIG_PERIOD_1000 1000
/*! @brief Período de refresco para el control de LEDs (en milisegundos). */
#define CONFIG_PERIOD_100 100
/*! @brief Período de refresco para la lectura de las teclas (en milisegundos). */
#define CONFIG_PERIOD_10 10

/*! @brief Variable auxiliar para activar o desactivar la medición y el control de LEDs. */
bool activar = true;
/*! @brief Variable auxiliar para mantener el último valor medido y congelar el estado de los LEDs. */
bool hold = false;
/*! @brief Almacena la distancia medida por el sensor en centímetros. */
float distancia = 0.00;

/*==================[internal data definition]===========================*/
/*! @brief Handle para la tarea de sensado de distancia. */
TaskHandle_t sensar_task_handle = NULL;
/*! @brief Handle para la tarea de control de LEDs. */
TaskHandle_t leds_task_handle = NULL;
/*! @brief Handle para la tarea de lectura de teclas. */
TaskHandle_t teclas_task_handle = NULL;

/*==================[internal functions declaration]=======================*/
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
		vTaskDelay(CONFIG_PERIOD_1000 / portTICK_PERIOD_MS);
	}
}

/*!
 * @brief Tarea que controla los LEDs según la distancia medida.
 *
 * Esta tarea enciende o apaga los LEDs dependiendo de la distancia almacenada
 * en la variable global `distancia`. También actualiza el display LCD.
 *
 * @param[in] pvParameter Puntero a los parámetros de la tarea.
 */
static void LedsTask(void *pvParameter){
	while(true){
		printf("Leds\n");
		if(activar){
			if(distancia < 10){
				LedsOffAll();
			}	
			else if( (distancia<20) & (distancia>10)){
				LedOn(LED_1);
				LedOff(LED_2);
				LedOff(LED_3);
			}
			else if((distancia<30) & (distancia>20)){
				LedOn(LED_1);
				LedOn(LED_2);
				LedOff(LED_3);
			}
			else if (distancia>30){
				LedOn(LED_1);
				LedOn(LED_2);
				LedOn(LED_3);
			}
			 if (!hold){
                LcdItsE0803Write(distancia);
            }
		}
		else{
			LcdItsE0803Off();
			LedsOffAll();
		}
		vTaskDelay(CONFIG_PERIOD_100 / portTICK_PERIOD_MS);
	}
}

/*!
 * @brief Tarea que lee las teclas (TEC1 y TEC2).
 *
 * Esta tarea detecta el estado de las teclas y activa o desactiva la medición
 * (`activar`), o mantiene el valor medido (`hold`), según sea necesario.
 *
 * @param[in] pvParameter Puntero a los parámetros de la tarea.
 */
static void TeclasTask(void *pvParameter){
	uint8_t teclas;
	while(true){
		printf("Teclas\n");
		teclas = SwitchesRead();
		switch (teclas){
		case SWITCH_1:
			activar = !activar;
			break;
		case SWITCH_2:
			hold = !hold;
			break;
		}
		vTaskDelay(CONFIG_PERIOD_10 / portTICK_PERIOD_MS);
	}
}

/*==================[external functions definition]========================*/
/*!
 * @brief Función principal de la aplicación.
 *
 * Esta función inicializa los periféricos y crea las tareas para el sensado de distancia,
 * control de LEDs y monitoreo de las teclas.
 */
void app_main(void){
	LedsInit();
	HcSr04Init(GPIO_3, GPIO_2);
	LcdItsE0803Init();
	SwitchesInit();

	xTaskCreate(&SensarTask, "Sensar", 1024, NULL, 4, &sensar_task_handle);
	xTaskCreate(&LedsTask, "Leds", 1024, NULL, 4, &leds_task_handle);	
	xTaskCreate(&TeclasTask, "Teclas", 1024, NULL, 4, &teclas_task_handle);
}
