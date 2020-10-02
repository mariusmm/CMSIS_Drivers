/*
 * Copyright (c) 2020 Màrius Montón <marius.monton@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Project:   CMSIS Driver implementation for STM32 devices
 *
 * The example is using
 * - I2C1 in using pins PB8, PB9 - CN7-2, CN7-4 (compatible with "NUCLEO-F446ZE")
 * - LSM9DS1 3D Accelerometer, 3D gyroscope, 3D magnetometer at address 0xD6 (https://www.st.com/resource/en/datasheet/lsm9ds1.pdf)
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "stm32f4xx_hal.h"

#include "Driver_I2C.h"

extern ARM_DRIVER_I2C Driver_I2C1;

/* LSM9DS1 I2C address */
const uint32_t dev_addr = 0xD6;

/* Receiver buffer */
uint8_t rx_buff[10];
uint8_t tx_buff[10];

void SystemClock_Config(void);
void Error_Handler(void);

volatile bool transfer_done = false;

void i2c_event(uint32_t event)
{
    if (event == ARM_I2C_EVENT_TRANSFER_DONE) {
        transfer_done = true;
    }
}

int main(void)
{
    HAL_Init();

    SystemClock_Config();

    /* Initialize the I2C driver */
    Driver_I2C1.Initialize(i2c_event);

    /* Power up the USART peripheral */
    Driver_I2C1.PowerControl(ARM_POWER_FULL);

    /* Configure I2C to 100 kHz bus speed */
    Driver_I2C1.Control(ARM_I2C_BUS_SPEED, ARM_I2C_BUS_SPEED_STANDARD);

    tx_buff[0] = 0x0F;          // "WHO AM I" register value should be 0x68
    Driver_I2C1.MasterTransmit(dev_addr, tx_buff, 1, true);

    /* Wait transfer to complete */
    while (transfer_done != true) ;
    transfer_done = false;

    Driver_I2C1.MasterReceive(dev_addr, rx_buff, 6, false);

    /* Wait transfer to complete */
    while (transfer_done != true) ;
    transfer_done = false;

    /* Chip detected, try write some registers and read them back to check I2C transfers */
    if (rx_buff[0] == 0x68) {

        memset(tx_buff, 0, 10);
        memset(rx_buff, 0, 10);

        /* Ensure I2C is available */
        while (Driver_I2C1.GetStatus().busy) ;

        /* Let's write registers 0x31 to 0x36 and read them back */
        tx_buff[0] = 0x31;
        tx_buff[1] = 0xBA;      /* 0x31 */
        tx_buff[2] = 0xD0;      /* 0x32 */
        tx_buff[3] = 0xCA;      /* 0x33 */
        tx_buff[4] = 0xFE;      /* 0x34 */
        tx_buff[5] = 0xDE;      /* 0x35 */
        tx_buff[6] = 0xAD;      /* 0x36 */
        Driver_I2C1.MasterTransmit(dev_addr, tx_buff, 7, false);

        /* Wait to finish transfer */
        while (Driver_I2C1.GetStatus().busy) ;

        /* Do a read register operation */
        tx_buff[0] = 0x31;
        Driver_I2C1.MasterTransmit(dev_addr, tx_buff, 1, true);
        while (Driver_I2C1.GetStatus().busy) ;

        Driver_I2C1.MasterReceive(dev_addr, rx_buff, 8, false);
        while (Driver_I2C1.GetStatus().busy) ;

        /* check that read back values are OK */
        volatile bool check_ok = true;
        for (int i = 0; i < 10; i++) {
            if (tx_buff[i + 1] != rx_buff[i]) {
                check_ok = false;
                break;
            }
        }
        /* Check here if check_ok is true. */
        while (1) {

        }
    }

    while (1) {

    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
    RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = { 0 };

          /** Configure the main internal regulator output voltage
	  */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
          /** Initializes the CPU, AHB and APB busses clocks
	  */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 4;
    RCC_OscInitStruct.PLL.PLLN = 168;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    RCC_OscInitStruct.PLL.PLLR = 2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }
          /** Initializes the CPU, AHB and APB busses clocks
	  */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_CLK48;
    PeriphClkInitStruct.Clk48ClockSelection = RCC_CLK48CLKSOURCE_PLLQ;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
        Error_Handler();
    }
}

void Error_Handler(void)
{

}
