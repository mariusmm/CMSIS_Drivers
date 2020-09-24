                                                                                                                                                                                                                                       /***************************************************************************//**
 * @file
 * @brief Simple LED Blink Demo for EFM32TG_STK3300
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include "em_device.h"
#include "em_chip.h"
#include "em_cmu.h"
#include "bsp.h"
#include "bsp_trace.h"

#include "Driver_USART.h"

volatile uint32_t msTicks;      /* counts 1ms timeTicks */

extern ARM_DRIVER_USART Driver_USART1;
static ARM_DRIVER_USART *USARTdrv = &Driver_USART1;

extern ARM_DRIVER_USART Driver_LEUART0;
static ARM_DRIVER_USART *LEUARTdrv = &Driver_LEUART0;

void Delay(uint32_t dlyTicks);

                                                                                                                                                                                                                                       /***************************************************************************//**
 * @brief SysTick_Handler
 * Interrupt Service Routine for system tick counter
 ******************************************************************************/
void SysTick_Handler(void)
{
    msTicks++;                  /* increment counter necessary in Delay() */
}

                                                                                                                                                                                                                                       /***************************************************************************//**
 * @brief Delays number of msTick Systicks (typically 1 ms)
 * @param dlyTicks Number of ticks to delay
 ******************************************************************************/
void Delay(uint32_t dlyTicks)
{
    uint32_t curTicks;

    curTicks = msTicks;
    while ((msTicks - curTicks) < dlyTicks) ;
}

                                                                                                                                                                                                                                       /***************************************************************************//**
 * @brief  Main function
 ******************************************************************************/
int main(void)
{
    /* Chip errata */
    CHIP_Init();

    /* If first word of user data page is non-zero, enable Energy Profiler trace */
    BSP_TraceProfilerSetup();

    /* Setup SysTick Timer for 1 msec interrupts  */
    if (SysTick_Config(CMU_ClockFreqGet(cmuClock_CORE) / 1000)) {
        while (1) ;
    }

    /* User must choose which clock source feeds low-frequency B clock branch */
    CMU_ClockSelectSet(cmuClock_LFB, cmuSelect_LFXO);

    /* Initialize LED driver */
    BSP_LedsInit();

    /*Initialize the USART driver */
    USARTdrv->Initialize(NULL);
    LEUARTdrv->Initialize(NULL);

    /*Power up the USART peripheral */
    USARTdrv->PowerControl(ARM_POWER_FULL);
    LEUARTdrv->PowerControl(ARM_POWER_FULL);

    /*Configure the USART to 115200 Bits/sec */
    USARTdrv->Control(ARM_USART_MODE_ASYNCHRONOUS |
                      ARM_USART_DATA_BITS_8 |
                      ARM_USART_PARITY_NONE | ARM_USART_STOP_BITS_1 |
                      ARM_USART_FLOW_CONTROL_NONE, 115200);

    LEUARTdrv->Control(ARM_USART_MODE_ASYNCHRONOUS |
                       ARM_USART_DATA_BITS_8 |
                       ARM_USART_PARITY_NONE | ARM_USART_STOP_BITS_1 |
                       ARM_USART_FLOW_CONTROL_NONE, 9600);

    /* Enable Receiver and Transmitter lines */
    USARTdrv->Control(ARM_USART_CONTROL_TX, 1);
    USARTdrv->Control(ARM_USART_CONTROL_RX, 1);

    LEUARTdrv->Control(ARM_USART_CONTROL_TX, 1);
    LEUARTdrv->Control(ARM_USART_CONTROL_RX, 1);

    /* Infinite blink loop */
    while (1) {
        BSP_LedToggle(0);
        USARTdrv->Send("Hello CMSIS\n", 12);
        LEUARTdrv->Send("Hello CMSIS\n", 12);
        Delay(1000);
    }
}
