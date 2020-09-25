# CMSIS_Drivers

Implementation of CMSIS Drivers for EFM32 (Silion Labs) and STM32 (ST Microelectronics) microcontrollers.

This project aims to implement all Drivers declared in CMSIS Drivers (https://github.com/ARM-software/CMSIS_5/tree/develop/CMSIS/Driver)

For each of the drivers implemented, there is a test that shows how to use it.

## EFM32
This library uses emlib (http://devtools.silabs.com/dl/documentation/doxygen/5.7/index.html).

Before use the library, user must set-up the clock tree properly (see EFM32/CMSIS_Driver_Test_UART.c for an example)

## STM32
This library uses the STM32 HAL (https://www.st.com/resource/en/user_manual/dm00105879-description-of-stm32f4-hal-and-ll-drivers-stmicroelectronics.pdf PDF).

Before use the library, user must set-up the clocks properly (see STM32/CMSIS_Driver_Test_USART.c for an example). It is not required to have bsp functions to set-up each device. This configuration can be done in each driver file.

## Using this project

Just commit the entire repository and import the right folder into your IDE or environment (EFM32/CMSIS_Driver/ or STM32/CMSIS_Driver/). 
In eclipse based IDEs (Simplicity Studio and STM32CubeIDE) a virtual folder can be set to point to the proper repository directory.

Documentation about the API is here: https://arm-software.github.io/CMSIS_5/Driver/html/index.html

## Modules implemented

* Driver_USART

## Modules to implement
* Driver_I2C
* Driver_SPI
* Driver_Flash.h
* Driver_Storage.h
* ...
