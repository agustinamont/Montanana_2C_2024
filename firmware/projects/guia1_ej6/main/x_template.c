/*! @mainpage Template
 *
 * @section genDesc General Description
 *
 * Es una función que recibe un dato de 32 bits, la cantidad de dígitos de salida y 
 * dos vectores de estructuras del tipo gpioConf_t. Uno de estos vectores es igual 
 * a uno definido previamente y el otro vector mapea los puertos con el dígito del 
 * LCD a donde mostrar un dato:
 * Dígito 1 -> GPIO_19
 * Dígito 2 -> GPIO_18
 * Dígito 3 -> GPIO_9
 * La función muestra por display el valor que recibe.
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
 * | 30/08/2024 | Document creation		                         |
 *
 * @author Agustina Montañana (agustina.montanana@ingenieria.uner.edu.ar)
 * 
 * @brief Implementación de funciones para mostrar números en un display LCD utilizando pines GPIO.
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include <gpio_mcu.h>
/*==================[macros and definitions]=================================*/
/**
 * @brief Número de bits necesarios para representar un dígito BCD.
 */
#define N_BITS 4

/**
 * @brief Número de dígitos del LCD.
 */
#define LCD_DIGITS 3 
/*==================[internal data definition]===============================*/
/**
 * @brief Estructura que representa la configuración de un pin GPIO.
 */

typedef struct {
    gpio_t pin;
    io_t dir;
} gpioConfig_t;

/*==================[internal functions declaration]=========================*/
/**
 * @brief Convierte un número entero en su representación en formato BCD.
 * 
 * @param[in] data Número entero a convertir.
 * @param[in] digits Número de dígitos para la representación BCD.
 * @param[out] bcd_number Array donde se almacenarán los dígitos BCD.
 * 
 * @return 0 en caso de éxito, -1 en caso de error.
 */
int8_t convertToBcdArray(uint32_t data, uint8_t digits, uint8_t *bcd_number);

/**
 * @brief Configura los pines GPIO para representar un dígito BCD.
 * 
 * @param[in] digit Dígito BCD a representar.
 * @param[in] gpio_config Configuración de los pines GPIO.
 */
void BCDtoGPIO(uint8_t digit, gpioConfig_t *gpio_config);

/**
 * @brief Inicializa un pin GPIO con la dirección especificada.
 * 
 * @param[in] pin Pin GPIO a inicializar.
 * @param[in] dir Dirección del pin (entrada o salida).
 */
void GPIOInit(gpio_t pin, io_t dir);

/**
 * @brief Enciende un pin GPIO.
 * 
 * @param[in] pin Pin GPIO a encender.
 */
void GPIOOn(gpio_t pin);

/**
 * @brief Apaga un pin GPIO.
 * 
 * @param[in] pin Pin GPIO a apagar.
 */
void GPIOOff(gpio_t pin);

/**
 * @brief Cambia el estado de un pin GPIO.
 * 
 * @param[in] pin Pin GPIO cuyo estado se cambiará.
 * @param[in] state Estado deseado (0 para apagado, 1 para encendido).
 */
void GPIOstate(gpio_t pin, uint8_t state);

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

// Implementación de BCDtoGPIO
void BCDtoGPIO(uint8_t digit, gpioConfig_t *gpio_config) {
    for (uint8_t i = 0; i < N_BITS; i++) {
        GPIOInit(gpio_config[i].pin, gpio_config[i].dir);
    }
    for (uint8_t i = 0; i < N_BITS; i++) {
            if ((digit & (1 << i)) == 0) {
                GPIOOff(gpio_config[i].pin);
            } else {
                GPIOOn(gpio_config[i].pin);
            }
        }
}

/**
 * @brief Muestra un número en el display LCD utilizando pines GPIO.
 * 
 * @param[in] data Número a mostrar en el display.
 * @param[in] data_gpio_config Configuración de los pines GPIO para los datos.
 * @param[in] digit_gpio_config Configuración de los pines GPIO para los dígitos.
 */
void displayNumberOnLCD(uint32_t data, gpioConfig_t *data_gpio_config, gpioConfig_t *digit_gpio_config) {
    uint8_t bcd_array[LCD_DIGITS];
    
    // Convertir el número a formato BCD
    if (convertToBcdArray(data, LCD_DIGITS, bcd_array) != 0) {
        printf("Error: el número es demasiado grande para la cantidad de dígitos.\n");
        return;
    }

    // Inicializar los pines del LCD
    for (uint8_t i = 0; i < LCD_DIGITS; i++) {
        GPIOInit(digit_gpio_config[i].pin, digit_gpio_config[i].dir);
    }

    // Mostrar cada dígito en el display
    for (uint8_t i = 0; i < LCD_DIGITS; i++) {
        // Apagar todos los dígitos
        for (uint8_t j = 0; j < LCD_DIGITS; j++) {
            GPIOOff(digit_gpio_config[j].pin);
        }
        
        // Configurar el dígito actual para que esté encendido
        GPIOOn(digit_gpio_config[i].pin);

// Mostrar el valor BCD del dígito
        BCDtoGPIO(bcd_array[i], data_gpio_config);
        

    }
}

/**
 * @brief Función principal de la aplicación.
 * 
 * Configura los pines GPIO y muestra un número en el display LCD.
 */
void app_main(void) {
    // Configuración de pines de datos y dígitos
    gpioConfig_t data_gpio_config[N_BITS] = {
        {GPIO_20, 1},
        {GPIO_21, 1},
        {GPIO_22, 1},
        {GPIO_23, 1}
    };
    
    gpioConfig_t digit_gpio_config[LCD_DIGITS] = {
        {GPIO_19, 1}, // Dígito 1
        {GPIO_18, 1}, // Dígito 2
        {GPIO_9, 1}   // Dígito 3
    };

    uint32_t number = 908; // Número a mostrar en el display

    displayNumberOnLCD(number, data_gpio_config, digit_gpio_config);
}
/*==================[end of file]============================================*/