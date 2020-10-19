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
 * Project:   MODBUS client based on CMSIS UART Driver for EFM32
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include "em_device.h"
#include "em_chip.h"
#include "em_cmu.h"
#include "bsp.h"
#include "bsp_trace.h"

#include "modbus_client.h"

extern ARM_DRIVER_USART Driver_LEUART0;

uint16_t my_registers[50];

volatile uint32_t msTicks;      /* counts 1ms timeTicks */

/**
 * @brief SysTick_Handler
 * Interrupt Service Routine for system tick counter
 */
void SysTick_Handler(void)
{
    msTicks++;                  /* increment counter necessary in Delay() */
}

uint32_t getSysTicks(void)
{
    return msTicks;
}

uint16_t read_register(uint16_t addr)
{
    return my_registers[addr];
}

void write_register(uint16_t addr, uint16_t data)
{
    my_registers[addr] = data;
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

    for (int i = 0; i < 50; i++) {
        my_registers[i] = i * 2;
    }

    MODBUS_Init(&Driver_LEUART0);

    do {
        do_MODBUS_Client(500);
    } while (1);
}
