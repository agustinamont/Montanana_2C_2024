/*! @mainpage Medidor de distancia por ultrasonido con interrupciones y puerto serie
 *
 * @section genDesc Descripción General
 *
 * Este programa mide la distancia utilizando un sensor de ultrasonido HC-SR04 conectado a una EDU-ESP. Muestra la distancia medida en un display LCD y, adicionalmente, envía los datos por puerto serie en formato ASCII. El control de las teclas se realiza mediante interrupciones, y se incluye la opción de activar/desactivar las mediciones y mantener el valor ("HOLD").
 *
 * El formato de salida al puerto serie es el siguiente: 
 * - 3 dígitos ASCII + 1 carácter espacio + dos caracteres para la unidad (cm) + salto de línea `\r\n`.
 *
 * La funcionalidad de las teclas es replicada a través de los comandos enviados por puerto serie ('O' para activar/desactivar y 'H' para la función de "HOLD").
 *
 * @section hardConn Conexiones de Hardware
 *
 * | Periférico        | EDU-ESP   |
 * |-------------------|-----------|
 * | Sensor HC-SR04     | GPIO_3, GPIO_2 |
 * | Display LCD        | EDU-ESP   |
 * | Tecla 1 (activar)  | EDU-ESP   |
 * | Tecla 2 (HOLD)     | EDU-ESP   |
 * | Puerto serie       | UART_PC   |
 *
 * @section changelog Historial de cambios
 *
 * | Fecha       | Descripción                                |
 * |-------------|--------------------------------------------|
 * | 20/09/2024  | Creación del documento                     |
 *
 * @section authors Autores
 *
 * - Agustina Montañana (agustina.montanana@ingenieria.uner.edu.ar)
 */

/*==================[inclusions]=============================================*/
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
#include "hc_sr04.h"
#include "gpio_mcu.h"
#include "lcditse0803.h"
#include "switch.h"
#include "timer_mcu.h"
#include "uart_mcu.h"

/*==================[macros and definitions]=================================*/
/*! @brief Período de refresco para la medición del sensor de ultrasonido.*/
#define REFRESH_MEDICION 1000000
/*! @brief Período de refresco para la actualización del display. */
#define REFRESH_DISPLAY 1000000

/*==================[internal data definition]===============================*/
/**
 * @brief Handle de la tarea encargada de sensar la distancia.
 * 
 * Esta variable guarda el identificador de la tarea de FreeRTOS que se encarga de leer la distancia desde el sensor de ultrasonido.
 */
TaskHandle_t Sensar_task_handle = NULL;

/**
 * @brief Handle de la tarea encargada de mostrar la distancia.
 * 
 * Esta variable guarda el identificador de la tarea de FreeRTOS que se encarga de actualizar los LEDs, el display y enviar la distancia por puerto serie.
 */
TaskHandle_t Mostrar_task_handle = NULL;

/*! @brief Variable que almacena la distancia medida por el sensor en centímetros. */
uint16_t distancia = 0;

/*! @brief Variable auxiliar para activar o desactivar la medición y el control de LEDs. */
bool activar = true;

/*! @brief Variable auxiliar para mantener el último valor medido y congelar el estado de los LEDs. */
bool hold = false;

/*==================[internal functions declaration]=========================*/
/**
 * @brief Enviar la distancia medida al puerto serie
 * 
 * Esta función envía el valor de la distancia al puerto serie con el formato especificado en la consigna.
 */
void escribirDistanciaEnPc(){
    UartSendString(UART_PC, "distancia ");
    UartSendString(UART_PC, (char *)UartItoa(distancia, 10));
    UartSendString(UART_PC, " cm\r\n");
}

/**
 * @brief Tarea para sensar la distancia
 * 
 * Esta tarea se activa por una interrupción de temporizador. Cuando está activada, mide la distancia con el sensor de ultrasonido.
 */
void sensarTask(){
    while (true){
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (activar){
            distancia = HcSr04ReadDistanceInCentimeters();
        }
    }
}

/**
 * @brief Tarea para mostrar la distancia
 * 
 * Esta tarea se activa por otra interrupción de temporizador. Controla los LEDs y el display LCD para mostrar la distancia medida.
 */
void mostrarTask(){
    while (true){
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (activar){
            if (distancia < 10){
                LedsOffAll();
            }
            else if ((distancia > 10) & (distancia < 20)){
                LedOn(LED_1);
                LedOff(LED_2);
                LedOff(LED_3);
            }
            else if ((distancia > 20) & (distancia < 30)){
                LedOn(LED_1);
                LedOn(LED_2);
                LedOff(LED_3);
            }
            else if (distancia > 30){
                LedOn(LED_1);
                LedOn(LED_2);
                LedOn(LED_3);
            }

            if (!hold){
			    LcdItsE0803Write(distancia);
            }
            escribirDistanciaEnPc();
        }
        else{
            LcdItsE0803Off();
            LedsOffAll();
        }
    }
}

/**
 * @brief Interrupción de la tecla 1
 * 
 * Activa o desactiva la medición cuando se presiona la tecla 1 o cuando se recibe el comando 'O' desde el puerto serie.
 */
void TeclaActivar(){
    activar = !activar;
}

/**
 * @brief Interrupción de la tecla 2
 * 
 * Activa o desactiva el modo "HOLD" cuando se presiona la tecla 2 o cuando se recibe el comando 'H' desde el puerto serie.
 */
void TeclaHold(){
    hold = !hold;
}

/**
 * @brief Función de interrupción para los comandos enviados desde el puerto serie
 * 
 * Esta función lee los comandos 'O' y 'H' desde el puerto serie, replicando la funcionalidad de las teclas 1 y 2.
 */
void TeclasOnHold(){
    uint8_t tecla;
    UartReadByte(UART_PC, &tecla);
    switch (tecla){
    case 'O':
        activar = !activar;
        break;
	case 'H':
		hold = !hold;
		break;
	}
}

/**
 * @brief Función de temporizador para la tarea de sensado
 * 
 * Activa la tarea de sensado utilizando una notificación.
 */
void FuncTimerSensar(){
    vTaskNotifyGiveFromISR(Sensar_task_handle, pdFALSE); 
}

/**
 * @brief Función de temporizador para la tarea de mostrar
 * 
 * Activa la tarea de mostrar utilizando una notificación.
 */
void FuncTimerMostrar(){
    vTaskNotifyGiveFromISR(Mostrar_task_handle, pdFALSE); /* Envía una notificación a la tarea asociada al mostrar */
}

/*==================[external functions definition]==========================*/
/**
 * @brief Función principal
 * 
 * Inicializa los periféricos, configura las teclas con interrupciones, los temporizadores, y las tareas de FreeRTOS para el control del sensor de ultrasonido y la visualización de la distancia.
 */
void app_main(void){
    LedsInit();
    HcSr04Init(GPIO_3, GPIO_2);
    LcdItsE0803Init();
    SwitchesInit();

    serial_config_t ControlarUart =
        {
            .port = UART_PC,
            .baud_rate = 115200,
            .func_p = TeclasOnHold,
            .param_p = NULL,
        };
    UartInit(&ControlarUart);
    /* Inicialización de timer medicion */
    timer_config_t timer_medicion = {
        .timer = TIMER_A,
        .period = REFRESH_MEDICION,
        .func_p = FuncTimerSensar,
        .param_p = NULL};
    TimerInit(&timer_medicion);

    /* Inicialización de timer mostrar */
    timer_config_t timer_mostrar = {
        .timer = TIMER_B,
        .period = REFRESH_DISPLAY,
        .func_p = FuncTimerMostrar,
        .param_p = NULL};
    TimerInit(&timer_mostrar);

    SwitchActivInt(SWITCH_1, TeclaActivar, NULL);
    SwitchActivInt(SWITCH_2, TeclaHold, NULL);

    xTaskCreate(&sensarTask, "sensar", 512, NULL, 5, &Sensar_task_handle);
    xTaskCreate(&mostrarTask, "mostrar", 512, NULL, 5, &Mostrar_task_handle);
}