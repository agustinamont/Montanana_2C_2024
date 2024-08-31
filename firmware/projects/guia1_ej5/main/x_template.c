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
 * | 12/09/2023 | Document creation		                         |
 *
 * @author Albano Peñalva (albano.penalva@uner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include "gpio_mcu.h"
/*==================[macros and definitions]=================================*/
const uint8_t N_BITS = 4;
/*==================[internal data definition]===============================*/
struct gpioConfig_t
{
	gpio_t pin;
	io_t dir;
};
/*==================[internal functions declaration]=========================*/
void BCDtoGPIO(uint8_t digit,  struct gpioConfig_t *gpio_config){
	for(uint8_t i = 0; i < N_BITS; i++){
		GPIOInit(gpio_config[i].pin, gpio_config[i].dir);
	}
	for(uint8_t i = 0; i < N_BITS; i++){
		if((digit & 1 << i) == 0){
			GPIOOff(gpio_config[i].pin);			
		}
		else{
			GPIOOn(gpio_config[i].pin);
		}
	}
}
/*==================[external functions definition]==========================*/
void app_main(void){
	
	uint8_t digit = 6;

	struct gpioConfig_t config_pines[N_BITS];

	config_pines[0].pin = GPIO_20;
	config_pines[1].pin = GPIO_21;
	config_pines[2].pin = GPIO_22;
	config_pines[3].pin = GPIO_23;

	for(uint8_t i = 0; i < N_BITS; i++)
	{
		config_pines[i].dir = 1;
	}

	BCDtoGPIO(digit, config_pines);


}
/*==================[end of file]============================================*/