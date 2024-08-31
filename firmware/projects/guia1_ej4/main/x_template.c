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
#include <stdbool.h>
/*==================[macros and definitions]=================================*/

/*==================[internal data definition]===============================*/

/*==================[internal functions declaration]=========================*/

/*==================[external functions definition]==========================*/
int8_t  convertToBcdArray (uint32_t data, uint8_t digits, uint8_t * bcd_number)
{
	if (digits > 10) {
        return -1;  // Error: el número de dígitos solicitado es mayor al máximo permitido.
    }
    // Inicializar el array de salida a 0
    for (uint8_t i = 0; i < digits; i++) {
        bcd_number[i] = 0;
    }
    for (uint8_t i = 0; i < digits; i++) {
        bcd_number[digits - i - 1] = data % 10;  // Guardar el dígito menos significativo
        data = data / 10;  // Eliminar el dígito menos significativo del dato
    }
    // Si después de convertir hay datos restantes en el número de entrada, 
    // significa que el número de dígitos era insuficiente
    if (data > 0) {
        return -1;  // Error: se necesitarían más dígitos para representar completamente el dato en BCD.
    }

    return 0;
}
void app_main(void){
	uint8_t bcd_array[5];
    uint32_t number = 123;
    uint8_t digits = 3;

    int8_t result = convertToBcdArray(number, digits, bcd_array);

    if (result == 0) {
        printf("BCD conversion successful: ");
        for (uint8_t i = 0; i < digits; i++) {
            printf("%d ", bcd_array[i]);
        }
        printf("\n");
    } else {
        printf("BCD conversion failed.\n");
    }
}
/*==================[end of file]============================================*/