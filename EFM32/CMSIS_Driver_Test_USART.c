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
 * Project:   CMSIS Driver implementation for EFM32 devices
 *
**/

#include <stdint.h>
#include <stdbool.h>
#include "em_device.h"
#include "em_chip.h"
#include "em_cmu.h"
#include "bsp.h"
#include "bsp_trace.h"

#include "Driver_USART.h"

volatile uint32_t msTicks;	/* counts 1ms timeTicks */

extern ARM_DRIVER_USART Driver_USART1;
static ARM_DRIVER_USART *USARTdrv = &Driver_USART1;

extern ARM_DRIVER_USART Driver_LEUART0;
static ARM_DRIVER_USART *LEUARTdrv = &Driver_LEUART0;

volatile char rec_buff[10];
volatile char lerec_buff[10];
/**
 * @brief SysTick_Handler
 * Interrupt Service Routine for system tick counter
 */
void SysTick_Handler(void)
{
	msTicks++;		/* increment counter necessary in Delay() */
}

/**
 * @brief Delays number of msTick Systicks (typically 1 ms)
 * @param dlyTicks Number of ticks to delay
 */
void Delay(uint32_t dlyTicks)
{
	uint32_t curTicks;

	curTicks = msTicks;
	while ((msTicks - curTicks) < dlyTicks) ;
}

void usart1_event(uint32_t event)
{
	if (event == ARM_USART_EVENT_RECEIVE_COMPLETE) {
		USARTdrv->Send((void *)rec_buff, 1);
	}
}

void leuart_event(uint32_t event)
{
	if (event == ARM_USART_EVENT_RECEIVE_COMPLETE) {
		LEUARTdrv->Send((void *)lerec_buff, 1);
	}
}

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
	USARTdrv->Initialize(usart1_event);
	LEUARTdrv->Initialize(leuart_event);

	/*Power up the USART peripheral */
	USARTdrv->PowerControl(ARM_POWER_FULL);
	LEUARTdrv->PowerControl(ARM_POWER_FULL);

	/*Configure the USART to 115200 Bits/sec */
	USARTdrv->Control(ARM_USART_MODE_ASYNCHRONOUS |
			  ARM_USART_DATA_BITS_8 |
			  ARM_USART_PARITY_NONE | ARM_USART_STOP_BITS_1 | ARM_USART_FLOW_CONTROL_NONE, 115200);

	LEUARTdrv->Control(ARM_USART_MODE_ASYNCHRONOUS |
			   ARM_USART_DATA_BITS_8 |
			   ARM_USART_PARITY_NONE | ARM_USART_STOP_BITS_1 | ARM_USART_FLOW_CONTROL_NONE, 9600);

	/* Enable Receiver and Transmitter lines */
	USARTdrv->Control(ARM_USART_CONTROL_TX, 1);
	USARTdrv->Control(ARM_USART_CONTROL_RX, 1);

	LEUARTdrv->Control(ARM_USART_CONTROL_TX, 1);
	LEUARTdrv->Control(ARM_USART_CONTROL_RX, 1);

	USARTdrv->Send("nHello CMSIS\n", 13);
	LEUARTdrv->Send("\nHello CMSIS\n", 13);

	USARTdrv->Send("Do echo\n", 8);
	LEUARTdrv->Send("Do echo\n", 8);

	while (1) {
		USARTdrv->Receive((void *)rec_buff, 1);
		LEUARTdrv->Receive((void *)lerec_buff, 1);
	}
}
