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
 * This library manages I2C for STM32 devices.
 * User should configure structs I2C1_Resources, I2C2_Resources, etc to
 * their routing and capabilities.
 *
 * Currently implemented:
 * - Implemented non-blocking mode for Master Transmit & Receive functions (IRQ based)
 *
 * To be implemented:
 * TODO: Implement Slave functions
 * TODO: Better error management
 *
 */
#include "Driver_I2C.h"

#include "stm32f4xx_hal.h"

#define ARM_I2C_DRV_VERSION    ARM_DRIVER_VERSION_MAJOR_MINOR(1, 0)     /* driver version */

/* Driver Version */
static const ARM_DRIVER_VERSION DriverVersion = {
    ARM_I2C_API_VERSION,
    ARM_I2C_DRV_VERSION
};

typedef struct {
    GPIO_TypeDef *port;
    GPIO_InitTypeDef pin;
} STM32_PIN;

typedef struct {
    ARM_I2C_CAPABILITIES capabilities;  // Capabilities
    /* Specific STM32 UART properties */
    I2C_HandleTypeDef instance;
    STM32_PIN SCLPin;
    STM32_PIN SDAPin;
    ARM_I2C_STATUS status;
    ARM_I2C_SignalEvent_t cb_event;
} STM32_I2C_RESOURCES;

#ifdef I2C1
static STM32_I2C_RESOURCES I2C1_Resources = {
    .capabilities = {0},
    .instance = {I2C1, {0}},
    .SCLPin = {GPIOB,
               {.Pin = GPIO_PIN_8,.Mode = GPIO_MODE_AF_OD,.Pull = GPIO_PULLUP,.Speed =
                GPIO_SPEED_FREQ_VERY_HIGH,.Alternate = GPIO_AF4_I2C1}},
    .SDAPin = {GPIOB,
               {.Pin = GPIO_PIN_9,.Mode = GPIO_MODE_AF_OD,.Pull = GPIO_PULLUP,.Speed =
                GPIO_SPEED_FREQ_VERY_HIGH,.Alternate = GPIO_AF4_I2C1}},
    .status = {0},
    .cb_event = NULL,
};
#endif

#ifdef I2C2
static STM32_I2C_RESOURCES I2C2_Resources = {
    .capabilities = {0},
    .instance = {I2C2, {0}},
    .SCLPin = {GPIOD,
               {.Pin = GPIO_PIN_5,.Mode = GPIO_MODE_AF_OD,.Pull = GPIO_PULLUP,.Speed =
                GPIO_SPEED_FREQ_VERY_HIGH,.Alternate = GPIO_AF7_USART1}},
    .SDAPin = {GPIOD,
               {.Pin = GPIO_PIN_6,.Mode = GPIO_MODE_AF_OD,.Pull = GPIO_PULLUP,.Speed =
                GPIO_SPEED_FREQ_VERY_HIGH,.Alternate = GPIO_AF7_USART1}},
    .status = {0},
    .cb_event = NULL,
};
#endif

#ifdef I2C3
static STM32_I2C_RESOURCES I2C3_Resources = {
    .capabilities = {0},
    .instance = {I2C3, {0}},
    .SCLPin = {GPIOD,
               {.Pin = GPIO_PIN_5,.Mode = GPIO_MODE_AF_OD,.Pull = GPIO_PULLUP,.Speed =
                GPIO_SPEED_FREQ_VERY_HIGH,.Alternate = GPIO_AF7_USART1}},
    .SDAPin = {GPIOD,
               {.Pin = GPIO_PIN_6,.Mode = GPIO_MODE_AF_OD,.Pull = GPIO_PULLUP,.Speed =
                GPIO_SPEED_FREQ_VERY_HIGH,.Alternate = GPIO_AF7_USART1}},
    .status = {0},
    .cb_event = NULL,
};
#endif

// STM32 functions
static int32_t STM_I2C_Initialize(ARM_I2C_SignalEvent_t cb_event, STM32_I2C_RESOURCES * i2c)
{

    if (i2c->instance.Instance == NULL) {
        return ARM_DRIVER_ERROR_PARAMETER;
    }
#ifdef I2C1
    else if (i2c->instance.Instance == I2C1) {
        __HAL_RCC_I2C1_CLK_ENABLE();
        HAL_NVIC_SetPriority(I2C1_ER_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(I2C1_EV_IRQn);
        HAL_NVIC_SetPriority(I2C1_ER_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(I2C1_ER_IRQn);

    }
#endif
#ifdef I2C2
    else if (i2c->instance.Instance == I2C2) {
        __HAL_RCC_I2C2_CLK_ENABLE();
        HAL_NVIC_EnableIRQ(I2C2_EV_IRQn);
        HAL_NVIC_EnableIRQ(I2C2_ER_IRQn);
    }
#endif
#ifdef I2C3
    else if (i2c->instance.Instance == I2C3) {
        __HAL_RCC_I2C3_CLK_ENABLE();
        HAL_NVIC_EnableIRQ(I2C3_EV_IRQn);
        HAL_NVIC_EnableIRQ(I2C3_ER_IRQn);
    }
#endif

    /* Default STM32 parameters */
    i2c->instance.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    i2c->instance.Init.ClockSpeed = 100000;     // 100 kHz
    i2c->instance.Init.DualAddressMode = I2C_DUALADDRESS_DISABLED;
    i2c->instance.Init.GeneralCallMode = I2C_GENERALCALL_DISABLED;
    i2c->instance.Init.NoStretchMode = I2C_NOSTRETCH_DISABLED;

    if (cb_event != NULL) {
        i2c->cb_event = cb_event;
    }

    HAL_GPIO_Init(i2c->SCLPin.port, &i2c->SCLPin.pin);
    HAL_GPIO_Init(i2c->SDAPin.port, &i2c->SDAPin.pin);

    return ARM_DRIVER_OK;
}

static int32_t STM_I2C_Uninitialize(STM32_I2C_RESOURCES * i2c)
{
    HAL_I2C_DeInit(&i2c->instance);

    HAL_GPIO_DeInit(i2c->SCLPin.port, i2c->SCLPin.pin.Pin);
    HAL_GPIO_DeInit(i2c->SDAPin.port, i2c->SDAPin.pin.Pin);

    return ARM_DRIVER_OK;
}

static int32_t STM_I2C_PowerControl(ARM_POWER_STATE state, STM32_I2C_RESOURCES * i2c)
{

    switch (state) {
    case ARM_POWER_OFF:
        HAL_I2C_DeInit(&i2c->instance);
        break;
    case ARM_POWER_LOW:
        return ARM_DRIVER_ERROR_UNSUPPORTED;
        break;
    case ARM_POWER_FULL:
        HAL_I2C_Init(&i2c->instance);
        break;
    }

    return ARM_DRIVER_OK;
}

static int32_t STM_I2C_MasterTransmit(uint32_t addr, const uint8_t * data, uint32_t num, bool xfer_pending,
                                      STM32_I2C_RESOURCES * i2c)
{
	if (HAL_I2C_Master_Transmit_IT(&i2c->instance, addr, (uint8_t*) data, num) == HAL_OK) {
		return ARM_DRIVER_OK;
	} else {
		return ARM_DRIVER_ERROR;
	}
}

static int32_t STM_I2C_MasterReceive(uint32_t addr, uint8_t * data, uint32_t num, bool xfer_pending,
                                     STM32_I2C_RESOURCES * i2c)
{
	if (HAL_I2C_Master_Receive_IT(&i2c->instance, addr, data, num) == HAL_OK) {
		return ARM_DRIVER_OK;
	} else {
		return ARM_DRIVER_ERROR;
	}
}

static int32_t STM_I2C_SlaveTransmit(const uint8_t * data, uint32_t num, STM32_I2C_RESOURCES * i2c)
{
    HAL_I2C_Slave_Transmit_IT(&i2c->instance, (uint8_t *) data, num);
    return ARM_DRIVER_OK;
}

static int32_t STM_I2C_SlaveReceive(uint8_t * data, uint32_t num, STM32_I2C_RESOURCES * i2c)
{
    HAL_I2C_Slave_Receive_IT(&i2c->instance, data, num);
    return ARM_DRIVER_OK;
}

static int32_t STM_I2C_GetDataCount(STM32_I2C_RESOURCES * i2c)
{
    return i2c->instance.XferCount;
}

static int32_t STM_I2C_Control(uint32_t control, uint32_t arg, STM32_I2C_RESOURCES * i2c)
{

    switch (control) {
    case ARM_I2C_OWN_ADDRESS:
        i2c->instance.Init.OwnAddress1 = arg;
        break;
    case ARM_I2C_BUS_SPEED:
        switch (arg) {
        case ARM_I2C_BUS_SPEED_STANDARD:
            i2c->instance.Init.ClockSpeed = 100000;
            break;
        case ARM_I2C_BUS_SPEED_FAST:
            i2c->instance.Init.ClockSpeed = 400000;
            break;
        case ARM_I2C_BUS_SPEED_FAST_PLUS:
        case ARM_I2C_BUS_SPEED_HIGH:
            return ARM_DRIVER_ERROR_UNSUPPORTED;
        }
        break;
    case ARM_I2C_BUS_CLEAR:
        break;
    case ARM_I2C_ABORT_TRANSFER:
        break;
    default:
        return ARM_DRIVER_ERROR_UNSUPPORTED;
    }

    HAL_I2C_Init(&i2c->instance);
    return ARM_DRIVER_OK;
}

static ARM_I2C_STATUS STM_I2C_GetStatus(STM32_I2C_RESOURCES * i2c)
{

    HAL_I2C_StateTypeDef i2c_status;

//    if (i2c == NULL) {
//        ARM_I2C_STATUS ret = { 0 };
//        return ret;
//    }
    i2c_status = HAL_I2C_GetState(&i2c->instance);

    if (i2c_status == HAL_I2C_STATE_READY) {
        i2c->status.busy = false;
    } else {
        i2c->status.busy = true;
    }

    if (i2c_status == HAL_I2C_STATE_ERROR) {
    	uint32_t i2c_error = HAL_I2C_GetError(&i2c->instance);
        i2c->status.bus_error = true;
    }

    return i2c->status;
}

static ARM_DRIVER_VERSION ARM_GetVersion(void)
{
    return DriverVersion;
}

// I2C1
#ifdef I2C1
static ARM_I2C_CAPABILITIES I2C1_GetCapabilities(void)
{
    return I2C1_Resources.capabilities;
}

static int32_t I2C1_Initialize(ARM_I2C_SignalEvent_t cb_event)
{
    return STM_I2C_Initialize(cb_event, &I2C1_Resources);
}

static int32_t I2C1_Uninitialize(void)
{
    return STM_I2C_Uninitialize(&I2C1_Resources);
}

static int32_t I2C1_PowerControl(ARM_POWER_STATE state)
{
    return STM_I2C_PowerControl(state, &I2C1_Resources);
}

static int32_t I2C1_MasterTransmit(uint32_t addr, const uint8_t * data, uint32_t num, bool xfer_pending)
{

    return STM_I2C_MasterTransmit(addr, data, num, xfer_pending, &I2C1_Resources);
}

static int32_t I2C1_MasterReceive(uint32_t addr, uint8_t * data, uint32_t num, bool xfer_pending)
{
    return STM_I2C_MasterReceive(addr, data, num, xfer_pending, &I2C1_Resources);
}

static int32_t I2C1_SlaveTransmit(const uint8_t * data, uint32_t num)
{
    return STM_I2C_SlaveTransmit(data, num, &I2C1_Resources);
}

static int32_t I2C1_SlaveReceive(uint8_t * data, uint32_t num)
{
    return STM_I2C_SlaveReceive(data, num, &I2C1_Resources);
}

static int32_t I2C1_GetDataCount(void)
{
    return STM_I2C_GetDataCount(&I2C1_Resources);
}

static int32_t I2C1_Control(uint32_t control, uint32_t arg)
{
    return STM_I2C_Control(control, arg, &I2C1_Resources);
}

static ARM_I2C_STATUS I2C1_GetStatus(void)
{
    return STM_I2C_GetStatus(&I2C1_Resources);
}

void I2C1_EV_IRQHandler(void)
{
    HAL_I2C_EV_IRQHandler(&I2C1_Resources.instance);
}

void I2C1_ER_IRQHandler(void)
{
    HAL_I2C_ER_IRQHandler(&I2C1_Resources.instance);
}
#endif

// I2C2
#ifdef I2C2
static ARM_I2C_CAPABILITIES I2C2_GetCapabilities(void)
{
    return I2C2_Resources.capabilities;
}

static int32_t I2C2_Initialize(ARM_I2C_SignalEvent_t cb_event)
{
    return STM_I2C_Initialize(cb_event, &I2C2_Resources);
}

static int32_t I2C2_Uninitialize(void)
{
    return STM_I2C_Uninitialize(&I2C2_Resources);
}

static int32_t I2C2_PowerControl(ARM_POWER_STATE state)
{
    return STM_I2C_PowerControl(state, &I2C2_Resources);
}

static int32_t I2C2_MasterTransmit(uint32_t addr, const uint8_t * data, uint32_t num, bool xfer_pending)
{

    return STM_I2C_MasterTransmit(addr, data, num, xfer_pending, &I2C2_Resources);
}

static int32_t I2C2_MasterReceive(uint32_t addr, uint8_t * data, uint32_t num, bool xfer_pending)
{
    return STM_I2C_MasterReceive(addr, data, num, xfer_pending, &I2C2_Resources);
}

static int32_t I2C2_SlaveTransmit(const uint8_t * data, uint32_t num)
{
    return STM_I2C_SlaveTransmit(data, num, &I2C2_Resources);
}

static int32_t I2C2_SlaveReceive(uint8_t * data, uint32_t num)
{
    return STM_I2C_SlaveReceive(data, num, &I2C2_Resources);
}

static int32_t I2C2_GetDataCount(void)
{
    return STM_I2C_GetDataCount(&I2C2_Resources);
}

static int32_t I2C2_Control(uint32_t control, uint32_t arg)
{
    return STM_I2C_Control(control, arg, &I2C2_Resources);
}

static ARM_I2C_STATUS I2C2_GetStatus(void)
{
    return STM_I2C_GetStatus(&I2C2_Resources);
}

void I2C2_EV_IRQHandler(void)
{
    HAL_I2C_EV_IRQHandler(&I2C2_Resources.instance);
}

void I2C2_ER_IRQHandler(void)
{
    HAL_I2C_ER_IRQHandler(&I2C2_Resources.instance);
}
#endif

// I2C3
#ifdef I2C3
static ARM_I2C_CAPABILITIES I2C3_GetCapabilities(void)
{
    return I2C3_Resources.capabilities;
}

static int32_t I2C3_Initialize(ARM_I2C_SignalEvent_t cb_event)
{
    return STM_I2C_Initialize(cb_event, &I2C3_Resources);
}

static int32_t I2C3_Uninitialize(void)
{
    return STM_I2C_Uninitialize(&I2C3_Resources);
}

static int32_t I2C3_PowerControl(ARM_POWER_STATE state)
{
    return STM_I2C_PowerControl(state, &I2C3_Resources);
}

static int32_t I2C3_MasterTransmit(uint32_t addr, const uint8_t * data, uint32_t num, bool xfer_pending)
{

    return STM_I2C_MasterTransmit(addr, data, num, xfer_pending, &I2C3_Resources);
}

static int32_t I2C3_MasterReceive(uint32_t addr, uint8_t * data, uint32_t num, bool xfer_pending)
{
    return STM_I2C_MasterReceive(addr, data, num, xfer_pending, &I2C3_Resources);
}

static int32_t I2C3_SlaveTransmit(const uint8_t * data, uint32_t num)
{
    return STM_I2C_SlaveTransmit(data, num, &I2C3_Resources);
}

static int32_t I2C3_SlaveReceive(uint8_t * data, uint32_t num)
{
    return STM_I2C_SlaveReceive(data, num, &I2C3_Resources);
}

static int32_t I2C3_GetDataCount(void)
{
    return STM_I2C_GetDataCount(&I2C3_Resources);
}

static int32_t I2C3_Control(uint32_t control, uint32_t arg)
{
    return STM_I2C_Control(control, arg, &I2C3_Resources);
}

static ARM_I2C_STATUS I2C3_GetStatus(void)
{
    return STM_I2C_GetStatus(&I2C3_Resources);
}

void I2C3_EV_IRQHandler(void)
{
    HAL_I2C_EV_IRQHandler(&I2C3_Resources.instance);
}

void I2C3_ER_IRQHandler(void)
{
    HAL_I2C_ER_IRQHandler(&I2C3_Resources.instance);
}
#endif

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef * hi2c)
{

#ifdef I2C1
    if (hi2c->Instance == I2C1_Resources.instance.Instance) {
        if (I2C1_Resources.cb_event != NULL) {
            I2C1_Resources.cb_event(ARM_I2C_EVENT_TRANSFER_DONE);
        }
    }
#endif
#ifdef I2C2
    if (hi2c->Instance == I2C2_Resources.instance.Instance) {
        if (I2C2_Resources.cb_event != NULL) {
            I2C2_Resources.cb_event(ARM_I2C_EVENT_TRANSFER_DONE);
        }
    }
#endif
#ifdef I2C3
    if (hi2c->Instance == I2C3_Resources.instance.Instance) {
        if (I2C3_Resources.cb_event != NULL) {
            I2C3_Resources.cb_event(ARM_I2C_EVENT_TRANSFER_DONE);
        }
    }
#endif
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef * hi2c)
{
#ifdef I2C1
    if (hi2c->Instance == I2C1_Resources.instance.Instance) {
        if (I2C1_Resources.cb_event != NULL) {
            I2C1_Resources.cb_event(ARM_I2C_EVENT_TRANSFER_DONE);
        }
    }
#endif
#ifdef I2C2
    if (hi2c->Instance == I2C2_Resources.instance.Instance) {
        if (I2C2_Resources.cb_event != NULL) {
            I2C2_Resources.cb_event(ARM_I2C_EVENT_TRANSFER_DONE);
        }
    }
#endif
#ifdef I2C3
    if (hi2c->Instance == I2C3_Resources.instance.Instance) {
        if (I2C3_Resources.cb_event != NULL) {
            I2C3_Resources.cb_event(ARM_I2C_EVENT_TRANSFER_DONE);
        }
    }
#endif
}

void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef * hi2c)
{
#ifdef I2C1
    if (hi2c->Instance == I2C1_Resources.instance.Instance) {
        if (I2C1_Resources.cb_event != NULL) {
            I2C1_Resources.cb_event(ARM_I2C_EVENT_TRANSFER_DONE);
        }
    }
#endif
#ifdef I2C2
    if (hi2c->Instance == I2C2_Resources.instance.Instance) {
        if (I2C2_Resources.cb_event != NULL) {
            I2C2_Resources.cb_event(ARM_I2C_EVENT_TRANSFER_DONE);
        }
    }
#endif
#ifdef I2C3
    if (hi2c->Instance == I2C3_Resources.instance.Instance) {
        if (I2C3_Resources.cb_event != NULL) {
            I2C3_Resources.cb_event(ARM_I2C_EVENT_TRANSFER_DONE);
        }
    }
#endif
}

void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef * hi2c)
{
#ifdef I2C1
    if (hi2c->Instance == I2C1_Resources.instance.Instance) {
        if (I2C1_Resources.cb_event != NULL) {
            I2C1_Resources.cb_event(ARM_I2C_EVENT_TRANSFER_DONE);
        }
    }
#endif
#ifdef I2C2
    if (hi2c->Instance == I2C2_Resources.instance.Instance) {
        if (I2C2_Resources.cb_event != NULL) {
            I2C2_Resources.cb_event(ARM_I2C_EVENT_TRANSFER_DONE);
        }
    }
#endif
#ifdef I2C3
    if (hi2c->Instance == I2C3_Resources.instance.Instance) {
        if (I2C3_Resources.cb_event != NULL) {
            I2C3_Resources.cb_event(ARM_I2C_EVENT_TRANSFER_DONE);
        }
    }
#endif
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef * hi2c)
{
    uint32_t event;
    uint32_t arm_event;

    event = HAL_I2C_GetError(hi2c);

    switch (event) {
    case HAL_I2C_ERROR_BERR:
        arm_event = ARM_I2C_EVENT_BUS_ERROR;
        break;
    case HAL_I2C_ERROR_ARLO:
        arm_event = ARM_I2C_EVENT_ARBITRATION_LOST;
        break;
    case HAL_I2C_ERROR_AF:
        arm_event = ARM_I2C_EVENT_ADDRESS_NACK;
        break;
    default:
        arm_event = ARM_I2C_EVENT_BUS_ERROR;
        break;
    }

#ifdef I2C1
    if (hi2c->Instance == I2C1_Resources.instance.Instance) {
        if (I2C1_Resources.cb_event != NULL) {
            I2C1_Resources.cb_event(arm_event);
        }
    }
#endif
#ifdef I2C2
    else if (hi2c->Instance == I2C2_Resources.instance.Instance) {
        if (I2C2_Resources.cb_event != NULL) {
            I2C2_Resources.cb_event(arm_event);
        }
    }
#endif
#ifdef I2C3
    else if (hi2c->Instance == I2C3_Resources.instance.Instance) {
        if (I2C3_Resources.cb_event != NULL) {
            I2C3_Resources.cb_event(arm_event);
        }
    }
#endif
}

void HAL_I2C_AbortCpltCallback(I2C_HandleTypeDef * hi2c)
{
    HAL_I2C_ErrorCallback(hi2c);
}

#ifdef I2C1
ARM_DRIVER_I2C Driver_I2C1 = {
    ARM_GetVersion,
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

#ifdef I2C2
ARM_DRIVER_I2C Driver_I2C2 = {
    ARM_GetVersion,
    I2C2_GetCapabilities,
    I2C2_Initialize,
    I2C2_Uninitialize,
    I2C2_PowerControl,
    I2C2_MasterTransmit,
    I2C2_MasterReceive,
    I2C2_SlaveTransmit,
    I2C2_SlaveReceive,
    I2C2_GetDataCount,
    I2C2_Control,
    I2C2_GetStatus
};
#endif

#ifdef I2C3
ARM_DRIVER_I2C Driver_I2C3 = {
    ARM_GetVersion,
    I2C3_GetCapabilities,
    I2C3_Initialize,
    I2C3_Uninitialize,
    I2C3_PowerControl,
    I2C3_MasterTransmit,
    I2C3_MasterReceive,
    I2C3_SlaveTransmit,
    I2C3_SlaveReceive,
    I2C3_GetDataCount,
    I2C3_Control,
    I2C3_GetStatus
};
#endif
