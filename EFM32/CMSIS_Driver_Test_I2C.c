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
 * - I2C1 in using pins PD7, PD6 - EXP-15, EXP-16
 * - LSM9DS1 3D Accelerometer, 3D gyroscope, 3D magnetometer at address 0xD6 (https://www.st.com/resource/en/datasheet/lsm9ds1.pdf)
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "em_device.h"
#include "em_chip.h"
#include "em_cmu.h"
#include "bsp.h"
#include "bsp_trace.h"
#include "em_i2c.h"

#include "Driver_I2C.h"

volatile uint32_t msTicks;      /* counts 1ms timeTicks */

extern ARM_DRIVER_I2C Driver_I2C0;

bool done = false;

/* LSM9DS1 I2C address */
const uint32_t dev_addr = 0xD6;

/* Receiver buffer */
uint8_t rx_buff[10];
uint8_t tx_buff[10];

volatile bool transfer_done = false;

/**
 * @brief SysTick_Handler
 * Interrupt Service Routine for system tick counter
 */
void SysTick_Handler(void)
{
    msTicks++;                  /* increment counter necessary in Delay() */
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

void i2c_event(uint32_t event)
{
    if (event == ARM_I2C_EVENT_TRANSFER_DONE) {
        transfer_done = true;
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

    /* Initialize the I2C driver */
    Driver_I2C0.Initialize(i2c_event);

    /* Power up the USART peripheral */
    Driver_I2C0.PowerControl(ARM_POWER_FULL);

    /* Configure I2C to 100 kHz bus speed */
    Driver_I2C0.Control(ARM_I2C_BUS_SPEED, ARM_I2C_BUS_SPEED_STANDARD);

    tx_buff[0] = 0x0F;          // "WHO AM I" register value should be 0x68
    Driver_I2C0.MasterTransmit(dev_addr, tx_buff, 1, true);

    /* Wait transfer to complete */
    while (transfer_done != true) ;
    transfer_done = false;

    Driver_I2C0.MasterReceive(dev_addr, rx_buff, 6, false);

    /* Wait transfer to complete */
    while (transfer_done != true) ;
    transfer_done = false;

    /* Chip detected, try write some registers and read them back to check I2C transfers */
    if (rx_buff[0] == 0x68) {

        memset(tx_buff, 0, 10);
        memset(rx_buff, 0, 10);

        /* Ensure I2C is available */
        while (Driver_I2C0.GetStatus().busy) ;

        /* Let's write registers 0x31 to 0x36 and read them back */
        tx_buff[0] = 0x31;
        tx_buff[1] = 0xBA;      /* 0x31 */
        tx_buff[2] = 0xD0;      /* 0x32 */
        tx_buff[3] = 0xCA;      /* 0x33 */
        tx_buff[4] = 0xFE;      /* 0x34 */
        tx_buff[5] = 0xDE;      /* 0x35 */
        tx_buff[6] = 0xAD;      /* 0x36 */
        Driver_I2C0.MasterTransmit(dev_addr, tx_buff, 7, false);

        /* Wait to finish transfer */
        while (Driver_I2C0.GetStatus().busy) ;

        /* Do a read register operation */
        tx_buff[0] = 0x31;
        Driver_I2C0.MasterTransmit(dev_addr, tx_buff, 1, true);
        while (Driver_I2C0.GetStatus().busy) ;

        Driver_I2C0.MasterReceive(dev_addr, rx_buff, 8, false);
        while (Driver_I2C0.GetStatus().busy) ;

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
