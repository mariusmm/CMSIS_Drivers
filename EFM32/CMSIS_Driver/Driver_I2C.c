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
 * This library manages I2C for EFM32 devices.
 *
 * - Implemented Master mode non-blocking mode using IRQ
 *
 * TODO: Implement Slave mode
 * TODO: Implement better ARM_I2C_GetStatus function.
 * TODO: Implement the use of DMA for Send, Receive and Transfer functions.
 *
 */

#include "Driver_I2C.h"

#include "em_device.h"
#include "em_cmu.h"
#include "em_i2c.h"
#include "em_dma.h"

#define ARM_I2C_DRV_VERSION ARM_DRIVER_VERSION_MAJOR_MINOR(2,4) /* driver version */

/* Driver Version */
static const ARM_DRIVER_VERSION DriverVersion = {
    ARM_I2C_API_VERSION,
    ARM_I2C_DRV_VERSION
};

typedef struct {
    ARM_I2C_SignalEvent_t cb_event;     // Event callback
    ARM_I2C_STATUS status;      // Status flags
    uint8_t flags;              // Control and state flags
    uint8_t sla_rw;             // Slave address and RW bit
    bool pending;               // Transfer pending (no STOP)
    uint8_t stalled;            // Stall mode status flags
    uint8_t con_aa;             // I2C slave CON flag
    int32_t cnt;                // Master transfer count
    uint8_t *data;              // Master data to transfer
    uint32_t num;               // Number of bytes to transfer
    uint8_t *sdata;             // Slave data to transfer
    uint32_t snum;              // Number of bytes to transfer
} I2C_CTRL;

typedef struct {
    GPIO_Port_TypeDef port;
    unsigned int pin;
} EFM32_PIN;

typedef struct {
    ARM_I2C_CAPABILITIES capabilities;
    void *device;
    I2C_Init_TypeDef i2c_cfg;   // I2C register interface
    uint32_t LOCATION;
    EFM32_PIN SCLPin;
    EFM32_PIN SDAPin;
    I2C_TransferSeq_TypeDef seq;
    ARM_I2C_STATUS status;
    ARM_I2C_SignalEvent_t cb_event;
} EFM32_I2C_RESOURCES;

static EFM32_I2C_RESOURCES I2C0_Resources = {
    {0},
    I2C0,
    {0},
    I2C_ROUTE_LOCATION_LOC1,
    {gpioPortD, 7},
    {gpioPortD, 6},
    {0},
    {0},
    NULL,
};

static ARM_DRIVER_VERSION I2C_GetVersion(void)
{
    return DriverVersion;
}

static int32_t EFM32_I2C_Initialize(ARM_I2C_SignalEvent_t cb_event, EFM32_I2C_RESOURCES * i2c)
{
    CMU_ClockEnable(cmuClock_GPIO, true);
    GPIO_PinModeSet(i2c->SCLPin.port, i2c->SCLPin.pin, gpioModeWiredAnd, 1);    /* SCL Pin */
    GPIO_PinModeSet(i2c->SDAPin.port, i2c->SDAPin.pin, gpioModeWiredAnd, 1);    /* SDA Pin */

    /* Set default values */
    i2c->i2c_cfg.enable = true;
    i2c->i2c_cfg.master = true;
    i2c->i2c_cfg.refFreq = 0;
    i2c->i2c_cfg.freq = I2C_FREQ_STANDARD_MAX;
    i2c->i2c_cfg.clhr = i2cClockHLRStandard;

#ifdef I2C0
    if (i2c->device == I2C0) {
        CMU_ClockEnable(cmuClock_I2C0, true);

        I2C_IntEnable(I2C0, I2C_IEN_TXC);
        NVIC_ClearPendingIRQ(I2C0_IRQn);
        NVIC_EnableIRQ(I2C0_IRQn);
    }
#endif
#ifdef I2C1
    if (i2c->device == I2C1) {
        CMU_ClockEnable(cmuClock_I2C1, true);

        I2C_IntEnable(I2C1, I2C_IEN_TXC);
        NVIC_ClearPendingIRQ(I2C1_IRQn);
        NVIC_EnableIRQ(I2C1_IRQn);
    }
#endif

    if (cb_event != NULL) {
        i2c->cb_event = cb_event;
    }

    return ARM_DRIVER_OK;
}

static int32_t EFM32_I2C_Uninitialize(EFM32_I2C_RESOURCES * i2c)
{

#ifdef I2C0
    if (i2c->device == I2C0) {
        I2C_IntDisable(I2C0, I2C_IEN_TXC);
        NVIC_DisableIRQ(I2C0_IRQn);
        CMU_ClockEnable(cmuClock_I2C0, false);
    }
#endif
#ifdef I2C1
    if (i2c->device == I2C1) {
        I2C_IntDisable(I2C1, I2C_IEN_TXC);
        NVIC_DisableIRQ(I2C1_IRQn);
        CMU_ClockEnable(cmuClock_I2C1, false);
    }
#endif

    GPIO_PinModeSet(i2c->SCLPin.port, i2c->SCLPin.pin, gpioModeDisabled, 1);    /* SCL Pin */
    GPIO_PinModeSet(i2c->SDAPin.port, i2c->SDAPin.pin, gpioModeDisabled, 1);    /* SDA Pin */

    i2c->cb_event = NULL;

    return ARM_DRIVER_OK;
}

static int32_t EFM32_I2C_PowerControl(ARM_POWER_STATE state, EFM32_I2C_RESOURCES * i2c)
{
    if (i2c->device == NULL) {
        return ARM_DRIVER_ERROR_PARAMETER;
    }

    switch (state) {
    case ARM_POWER_OFF:
        I2C_IntDisable(i2c->device, I2C_IEN_TXC);
        I2C_Enable(i2c->device, false);
        break;
    case ARM_POWER_LOW:
        return ARM_DRIVER_ERROR_UNSUPPORTED;
        break;
    case ARM_POWER_FULL:

#ifdef I2C0
        if (i2c->device == I2C0) {
            NVIC_ClearPendingIRQ(I2C0_IRQn);
            NVIC_EnableIRQ(I2C0_IRQn);
        }
#endif
#ifdef I2C1
        if (i2c->device == I2C1) {
            NVIC_ClearPendingIRQ(I2C1_IRQn);
            NVIC_EnableIRQ(I2C1_IRQn);
        }
#endif

        /* Enable TX Complete IRQ */
        I2C_IntEnable(i2c->device, I2C_IEN_TXC);

        ((I2C_TypeDef *) i2c->device)->ROUTE = I2C_ROUTE_SDAPEN | I2C_ROUTE_SCLPEN | i2c->LOCATION;

        I2C_Enable(i2c->device, true);
        break;
    }

    return ARM_DRIVER_OK;
}

static int32_t EFM32_I2C_MasterTransmit(uint32_t addr, const uint8_t * data, uint32_t num, bool xfer_pending,
                                        EFM32_I2C_RESOURCES * i2c)
{

    I2C_TransferReturn_TypeDef ret;
    i2c->seq.addr = addr;
    i2c->seq.buf[0].data = (uint8_t *) data;
    i2c->seq.buf[0].len = num;
    i2c->seq.flags = I2C_FLAG_WRITE;

    ret = I2C_TransferInit(i2c->device, &i2c->seq);

    if (ret >= 0) {
        return ARM_DRIVER_OK;
    } else {
        return ARM_DRIVER_ERROR;
    }
}

static int32_t EFM32_I2C_MasterReceive(uint32_t addr, const uint8_t * data, uint32_t num, bool xfer_pending,
                                       EFM32_I2C_RESOURCES * i2c)
{

    I2C_TransferReturn_TypeDef ret;
    i2c->seq.addr = addr;
    i2c->seq.buf[0].data = (uint8_t *) data;
    i2c->seq.buf[0].len = num;
    i2c->seq.flags = I2C_FLAG_READ;

    ret = I2C_TransferInit(i2c->device, &i2c->seq);
    if (ret >= 0) {
        return ARM_DRIVER_OK;
    } else {
        return ARM_DRIVER_ERROR;
    }
}

static int32_t EFM32_I2C_SlaveTransmit(const uint8_t * data, uint32_t num, EFM32_I2C_RESOURCES * i2c)
{
    return ARM_DRIVER_ERROR_UNSUPPORTED;
}

static int32_t EFM32_I2C_SlaveReceive(const uint8_t * data, uint32_t num, EFM32_I2C_RESOURCES * i2c)
{
    return ARM_DRIVER_ERROR_UNSUPPORTED;
}

static int32_t EFM32_I2C_GetDataCount(EFM32_I2C_RESOURCES * i2c)
{
    /* EFM32 device has not HW control over bits send/received */
    return -1;
}

static int32_t EFM32_I2C_Control(uint32_t control, uint32_t arg, EFM32_I2C_RESOURCES * i2c)
{
    switch (control) {
    case ARM_I2C_OWN_ADDRESS:
        return ARM_DRIVER_ERROR_UNSUPPORTED;
    case ARM_I2C_BUS_SPEED:
        switch (arg) {
        case ARM_I2C_BUS_SPEED_STANDARD:
            i2c->i2c_cfg.freq = 100000; // 100 kHz
            break;
        case ARM_I2C_BUS_SPEED_FAST:
            i2c->i2c_cfg.freq = 400000; // 400 kHz
            break;
        case ARM_I2C_BUS_SPEED_FAST_PLUS:
            i2c->i2c_cfg.freq = 1000000;        // 1 MHz
            break;
        case ARM_I2C_BUS_SPEED_HIGH:
            i2c->i2c_cfg.freq = 3400000;        // 3.4 MHz
        }
        break;
    case ARM_I2C_BUS_CLEAR:
        break;
    case ARM_I2C_ABORT_TRANSFER:
        break;
    default:
        return ARM_DRIVER_ERROR_UNSUPPORTED;
    }

    I2C_Init(i2c->device, &i2c->i2c_cfg);
    return ARM_DRIVER_OK;
}

static ARM_I2C_STATUS EFM32_I2C_GetStatus(EFM32_I2C_RESOURCES * i2c)
{
    uint32_t state;

    state = ((I2C_TypeDef *) i2c->device)->STATE;

    if (state & I2C_STATE_BUSY) {
        i2c->status.busy = 1;
    } else {
        i2c->status.busy = 0;
    }

    if (state & I2C_STATE_MASTER) {
        i2c->status.mode = 1;
    } else {
        i2c->status.mode = 0;
    }

    return i2c->status;
}

static void I2C_IRQHandler(EFM32_I2C_RESOURCES * i2c)
{
    I2C_TransferReturn_TypeDef status;

    status = I2C_Transfer(i2c->device);

    if ((status == i2cTransferDone) && (i2c->cb_event != NULL)) {
        i2c->cb_event(ARM_I2C_EVENT_TRANSFER_DONE);
    }
}

#ifdef I2C0
/* I2C0 Driver wrapper functions */
static ARM_I2C_CAPABILITIES I2C0_GetCapabilities(void)
{
    return I2C0_Resources.capabilities;
}

static int32_t I2C0_Initialize(ARM_I2C_SignalEvent_t cb_event)
{
    return EFM32_I2C_Initialize(cb_event, &I2C0_Resources);
}

static int32_t I2C0_Uninitialize(void)
{
    return EFM32_I2C_Uninitialize(&I2C0_Resources);
}

static int32_t I2C0_PowerControl(ARM_POWER_STATE state)
{
    return EFM32_I2C_PowerControl(state, &I2C0_Resources);
}

static int32_t I2C0_MasterTransmit(uint32_t addr, const uint8_t * data, uint32_t num, bool xfer_pending)
{
    return EFM32_I2C_MasterTransmit(addr, data, num, xfer_pending, &I2C0_Resources);
}

static int32_t I2C0_MasterReceive(uint32_t addr, uint8_t * data, uint32_t num, bool xfer_pending)
{
    return EFM32_I2C_MasterReceive(addr, data, num, xfer_pending, &I2C0_Resources);
}

static int32_t I2C0_SlaveTransmit(const uint8_t * data, uint32_t num)
{
    return EFM32_I2C_SlaveTransmit(data, num, &I2C0_Resources);
}

static int32_t I2C0_SlaveReceive(uint8_t * data, uint32_t num)
{
    return EFM32_I2C_SlaveReceive(data, num, &I2C0_Resources);
}

static int32_t I2C0_GetDataCount(void)
{
    return EFM32_I2C_GetDataCount(&I2C0_Resources);
}

static int32_t I2C0_Control(uint32_t control, uint32_t arg)
{
    return EFM32_I2C_Control(control, arg, &I2C0_Resources);
}

static ARM_I2C_STATUS I2C0_GetStatus(void)
{
    return EFM32_I2C_GetStatus(&I2C0_Resources);
}

void I2C0_IRQHandler(void)
{
    I2C_IRQHandler(&I2C0_Resources);
}
#endif

#ifdef I2C1
/* I2C1 Driver wrapper functions */
static ARM_I2C_CAPABILITIES I2C1_GetCapabilities(void)
{
    return I2C1_Resources.capabilities;
}

static int32_t I2C1_Initialize(ARM_I2C_SignalEvent_t cb_event)
{
    return EFM32_I2C_Initialize(cb_event, &I2C1_Resources);
}

static int32_t I2C1_Uninitialize(void)
{
    return EFM32_I2C_Uninitialize(&I2C1_Resources);
}

static int32_t I2C1_PowerControl(ARM_POWER_STATE state)
{
    return EFM32_I2C_PowerControl(state, &I2C1_Resources);
}

static int32_t I2C1_MasterTransmit(uint32_t addr, const uint8_t * data, uint32_t num, bool xfer_pending)
{
    return EFM32_I2C_MasterTransmit(addr, data, num, xfer_pending, &I2C1_Resources);
}

static int32_t I2C1_MasterReceive(uint32_t addr, uint8_t * data, uint32_t num, bool xfer_pending)
{
    return EFM32_I2C_MasterReceive(addr, data, num, xfer_pending, &I2C1_Resources);
}

static int32_t I2C1_SlaveTransmit(const uint8_t * data, uint32_t num)
{
    return EFM32_I2C_SlaveTransmit(data, num, &I2C1_Resources);
}

static int32_t I2C1_SlaveReceive(uint8_t * data, uint32_t num)
{
    return EFM32_I2C_SlaveReceive(data, num, &I2C1_Resources);
}

static int32_t I2C1_GetDataCount(void)
{
    return EFM32_I2C_GetDataCount(&I2C1_Resources);
}

static int32_t I2C1_Control(uint32_t control, uint32_t arg)
{
    return EFM32_I2C_Control(control, arg, &I2C1_Resources);
}

static ARM_I2C_STATUS I2C1_GetStatus(void)
{
    return EFM32_I2C_GetStatus(&I2C1_Resources);
}

void I2C1_IRQHandler(void)
{
    I2C_IRQHandler(&I2C1_Resources);
}
#endif

#ifdef I2C0
ARM_DRIVER_I2C Driver_I2C0 = {
    I2C_GetVersion,
    I2C0_GetCapabilities,
    I2C0_Initialize,
    I2C0_Uninitialize,
    I2C0_PowerControl,
    I2C0_MasterTransmit,
    I2C0_MasterReceive,
    I2C0_SlaveTransmit,
    I2C0_SlaveReceive,
    I2C0_GetDataCount,
    I2C0_Control,
    I2C0_GetStatus
};
#endif
#ifdef I2C1
ARM_DRIVER_I2C Driver_I2C1 = {
    I2C_GetVersion,
    I2C1_GetCapabilities,
    I2C1_Initialize,
    I2C1_Uninitialize,
    I2C1_PowerControl,
    I2C1_MasterTransmit,
    I2C1_MasterReceive,
    I2C1_SlaveTransmit,
    I2C1_SlaveReceive,
    I2C1_GetDataCount,
    I2C1_Control,
    I2C1_GetStatus
};
#endif
