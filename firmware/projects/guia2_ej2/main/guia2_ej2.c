/*! @mainpage Proyecto: Medidor de Distancia por Ultrasonido con Interrupciones
 *
 * @section genDesc Descripción General
 *
 * Este firmware modifica el proyecto original de medidor de distancia por ultrasonido, añadiendo el uso de interrupciones para controlar las teclas y timers. La distancia medida se utiliza para controlar un conjunto de LEDs, y se muestra en un display LCD.
 * 
 * - TEC1 activa o desactiva la medición de distancia mediante una interrupción.
 * - TEC2 mantiene o libera el último valor medido (HOLD) mediante una interrupción.
 * - Se usa un timer para generar una interrupción que refresca la medición cada 1 segundo.
 *
 * @section funcDesc Descripción Funcional
 *
 * Los LEDs operan de acuerdo a la siguiente lógica:
 * - Distancia < 10 cm: Apagar todos los LEDs.
 * - 10 cm <= Distancia < 20 cm: Encender LED_1.
 * - 20 cm <= Distancia < 30 cm: Encender LED_1 y LED_2.
 * - Distancia >= 30 cm: Encender LED_1, LED_2 y LED_3.
 *
 * La distancia actual también se muestra en el display LCD en centímetros, a menos que esté activado el modo "HOLD" (TEC2).
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
 * | TEC1 (SWITCH_1)| GPIO_X        |
 * | TEC2 (SWITCH_2)| GPIO_X        |
 *
 * @section changelog Historial de Cambios
 *
 * |    Fecha      | Descripción                                    |
 * |:-------------:|:-----------------------------------------------|
 * | 13/09/2024    | Creación de la documentación                   |
 *
 * @section author Autor
 * - Agustina Montañana (agustina.montanana@ingenieria.uner.edu.ar)
 */
/*==================[inclusiones]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include "hc_sr04.h"
#include "lcditse0803.h"
#include <led.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "switch.h"
#include "timer_mcu.h"

/*==================[macros and definitions]===================================*/
/*! @brief Período de refresco del sensor (en microsegundos). */
#define CONFIG_SENSOR_TIMERA 1000000
/*! @brief Variable que almacena la distancia medida por el sensor en centímetros. */
uint16_t distancia;
/*! @brief Variable auxiliar para activar o desactivar la medición y el control de LEDs. */
bool activar = true;
/*! @brief Variable auxiliar para mantener el último valor medido y congelar el estado de los LEDs. */
bool hold = false;

/*==================[internal data definition]===========================*/
/*! @brief Handle para la tarea de operación con la distancia medida. */
TaskHandle_t operar_distancia_task_handle = NULL;

/*==================[internal functions declaration]=======================*/
/*!
 * @brief Función que maneja la interrupción generada por el temporizador (Timer A).
 *
 * Esta función envía una notificación a la tarea que opera con la distancia cuando el
 * temporizador genera una interrupción, indicando que puede continuar su operación.
 *
 * @param[in] param Parámetro no utilizado.
 */
void FuncTimerA(void* param){
	vTaskNotifyGiveFromISR(operar_distancia_task_handle, pdFALSE);
}

/*!
 * @brief Tarea que opera con la distancia medida y controla los LEDs y el LCD.
 *
 * Esta tarea se ejecuta al recibir una notificación del temporizador, lee la distancia
 * medida, controla los LEDs según dicha distancia, y actualiza el display LCD si no está
 * activado el modo "HOLD".
 *
 * @param[in] pvParameter Puntero a los parámetros de la tarea.
 */
static void OperarConDistancia(void *pvParameter){
    while(true){
		// La tarea esta en espera (bloqueada) hasta que reciba una notificación mediante ulTaskNotifyTake
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  
		
		if(activar){
			// Medir distancia
			distancia = HcSr04ReadDistanceInCentimeters();

			// Manejar LEDs según la distancia
			if(distancia < 10){
				LedOff(LED_1);
				LedOff(LED_2);
				LedOff(LED_3);
			}
			else if (distancia >= 10 && distancia < 20){
				LedOn(LED_1);
				LedOff(LED_2);
				LedOff(LED_3);
			}
			else if(distancia >= 20 && distancia < 30){
				LedOn(LED_1);
				LedOn(LED_2);
				LedOff(LED_3);
			}
			else if(distancia >= 30){
				LedOn(LED_1);
				LedOn(LED_2);
				LedOn(LED_3);
			}

			// Mostrar distancia en la pantalla LCD si no está en hold
			if(hold){
				LcdItsE0803Write(distancia);
			}
		}
		else{
			// Apagar LEDs y pantalla LCD si on está apagado
			LedOff(LED_1);
			LedOff(LED_2);
			LedOff(LED_3);
			LcdItsE0803Off();
		}
		//vTaskDelay(CONFIG_DELAY / portTICK_PERIOD_MS);
    }
}

/*!
 * @brief Interrupción para la tecla TEC1 (SWITCH_1).
 *
 * Esta función cambia el estado de activación de la medición cuando se presiona la tecla TEC1.
 */
static void Interrupciontecla1(void){
		
		activar = !activar;
}

/*!
 * @brief Interrupción para la tecla TEC2 (SWITCH_2).
 *
 * Esta función activa o desactiva el modo "HOLD" cuando se presiona la tecla TEC2.
 */
static void Interrupciontecla2(void){
		
		hold = !hold;
}

/*==================[external functions definition]========================*/
/*!
 * @brief Función principal de la aplicación.
 *
 * Esta función inicializa los periféricos (LEDs, sensor ultrasónico, LCD, teclas),
 * configura el temporizador y las interrupciones, y crea las tareas para operar con
 * la distancia medida.
 */
void app_main(void){
	LedsInit();
	HcSr04Init(3, 2);
	LcdItsE0803Init();
	SwitchesInit();
	// timer_config_t: configura el temporizador definiendo su periodo y la función que se llamará cuando el temporizador 
	//se dispare.
	timer_config_t timer_sensor = {
        .timer = TIMER_A,
        .period = CONFIG_SENSOR_TIMERA,
        .func_p = FuncTimerA,
        .param_p = NULL
    };
	TimerInit(&timer_sensor);
	// Se habilitan interrupciones cuando se presionan las teclas SWITCH_1 y SWITCH_2 (desencadena OperarConTeclado)
	SwitchActivInt(SWITCH_1, &Interrupciontecla1, NULL );
	SwitchActivInt(SWITCH_2, &Interrupciontecla2, NULL );

	xTaskCreate(&OperarConDistancia, "OperarConDistancia", 2048, NULL, 5, &operar_distancia_task_handle);
	
	TimerStart(timer_sensor.timer);
}
