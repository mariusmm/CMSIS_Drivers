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
 * - USART2 in pins PD5, PP6 - CN9-4, CN9-6 (compatible with "NUCLEO-F446ZE")
 * - UART5 in pins PC12, PD2 - CN8-10, CN8-12 (compatible with "NUCLEO-F446ZE")
 */

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

#include "Driver_USART.h"

extern ARM_DRIVER_USART Driver_USART2;
static ARM_DRIVER_USART *USARTdrv = &Driver_USART2;

extern ARM_DRIVER_USART Driver_UART5;

void SystemClock_Config(void);
void Error_Handler(void);

char uart2_buff[10];
char uart5_buff[10];

void usart2_cb(uint32_t event)
{
    if (event == ARM_USART_EVENT_RECEIVE_COMPLETE) {
        Driver_USART2.Send((void *)uart2_buff, 1);
    }
}

void usart5_cb(uint32_t event)
{
    if (event == ARM_USART_EVENT_RECEIVE_COMPLETE) {
        Driver_UART5.Send((void *)uart5_buff, 1);
    }
}

                                                                                                                                                                                                                                                                                                                    /***************************************************************************//**
 * @brief  Main function
 ******************************************************************************/
int main(void)
{
    HAL_Init();

    SystemClock_Config();

    /* Initialize the USART driver */
    USARTdrv->Initialize(usart2_cb);

    /* Power up the USART peripheral */
    USARTdrv->PowerControl(ARM_POWER_FULL);

    /* Configure the USART to 115200 Bits/sec */
    USARTdrv->Control(ARM_USART_MODE_ASYNCHRONOUS |
                      ARM_USART_DATA_BITS_8 |
                      ARM_USART_PARITY_NONE | ARM_USART_STOP_BITS_1 | ARM_USART_FLOW_CONTROL_NONE, 115200);

    /* Enable Receiver and Transmitter lines */
    USARTdrv->Control(ARM_USART_CONTROL_TX, 1);
    USARTdrv->Control(ARM_USART_CONTROL_RX, 1);

    /* UART5 */
    Driver_UART5.Initialize(usart5_cb);
    Driver_UART5.PowerControl(ARM_POWER_FULL);
    Driver_UART5.Control(ARM_USART_MODE_ASYNCHRONOUS |
                         ARM_USART_DATA_BITS_8 |
                         ARM_USART_PARITY_NONE | ARM_USART_STOP_BITS_1 | ARM_USART_FLOW_CONTROL_NONE, 115200);
    Driver_UART5.Control(ARM_USART_CONTROL_TX, 1);
    Driver_UART5.Control(ARM_USART_CONTROL_RX, 1);

    USARTdrv->Send("\nHello CMSIS\n", 13);
    Driver_UART5.Send("\nHello CMSIS UART5\n", 19);

    USARTdrv->Send("Do echo\n", 8);
    Driver_UART5.Send("Do echo\n", 8);

    while (1) {
        USARTdrv->Receive((void *)uart2_buff, 1);
        Driver_UART5.Receive((void *)uart5_buff, 1);
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
