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
 * This library manages USARTs and LEUARTs for EFM32 devices.
 * The example is using USART1 in LOCATION #1 and LEUART0 in LOCATION #0
 * (compatible with "Starter Kit EFM32TG-STK3300") pins EXP-4, EXP-6 and EXP-12,
 * EXP-14,
 *
 * - Implemented non-blocking mode for Send, Receive functions
 *
 * TODO: Implement transfer function
 * TODO: Implement the use of DMA for Send, Receive and Transfer functions.
 * TODO: Implement ARM_USART_GetStatus function.
 * TODO: Implement ARM_USART_SetModemControl function
 * TODO: Implement ARM_USART_GetModemStatus function
 *
 */

#include "Driver_USART.h"

#include "em_device.h"
#include "em_cmu.h"
#include "em_usart.h"
#include "em_leuart.h"
#include "em_dma.h"

#define ARM_USART_DRV_VERSION    ARM_DRIVER_VERSION_MAJOR_MINOR(1, 0)	/* driver version */

/* Driver Version */
static const ARM_DRIVER_VERSION DriverVersion = {
	ARM_USART_API_VERSION,
	ARM_USART_DRV_VERSION
};

typedef struct {
	GPIO_Port_TypeDef port;
	unsigned int pin;
} EFM32_PIN;

typedef struct {
	void *TxBuf;		/* Pointer to Tx buffer */
	void *RxBuf;		/* Pointer to Rx Buffer */
	uint32_t TxNum;		/* Items to send */
	uint32_t RxNum;		/* Items to receive */
	uint32_t TxCnt;		/* Items sent */
	uint32_t RxCnt;		/* Items received */
} USART_TRANSFER_INFO;

typedef struct {
	ARM_USART_CAPABILITIES capabilities;	// Capabilities
	/* Specific EFM32 UART properties */
	void *device;
	USART_InitAsync_TypeDef usart_cfg;
	uint32_t LOCATION;
	EFM32_PIN TxPin;
	EFM32_PIN RxPin;
	USART_TRANSFER_INFO xfer;
	ARM_USART_STATUS status;
	ARM_USART_SignalEvent_t cb_event;
} EFM32_USART_RESOURCES;

/* Driver Capabilities */
static EFM32_USART_RESOURCES USART0_Resources = {
	{
	 1,			/* supports UART (Asynchronous) mode */
	 0,			/* supports Synchronous Master mode */
	 0,			/* supports Synchronous Slave mode */
	 0,			/* supports UART Single-wire mode */
	 0,			/* supports UART IrDA mode */
	 0,			/* supports UART Smart Card mode */
	 0,			/* Smart Card Clock generator available */
	 0,			/* RTS Flow Control available */
	 0,			/* CTS Flow Control available */
	 0,			/* Transmit completed event: \ref ARM_USART_EVENT_TX_COMPLETE */
	 0,			/* Signal receive character timeout event: \ref ARM_USART_EVENT_RX_TIMEOUT */
	 0,			/* RTS Line: 0=not available, 1=available */
	 0,			/* CTS Line: 0=not available, 1=available */
	 0,			/* DTR Line: 0=not available, 1=available */
	 0,			/* DSR Line: 0=not available, 1=available */
	 0,			/* DCD Line: 0=not available, 1=available */
	 0,			/* RI Line: 0=not available, 1=available */
	 0,			/* Signal CTS change event: \ref ARM_USART_EVENT_CTS */
	 0,			/* Signal DSR change event: \ref ARM_USART_EVENT_DSR */
	 0,			/* Signal DCD change event: \ref ARM_USART_EVENT_DCD */
	 0,			/* Signal RI change event: \ref ARM_USART_EVENT_RI */
	 0
	 /* Reserved (must be zero) */
	 },
	USART0,
	USART_INITASYNC_DEFAULT,
	USART_ROUTE_LOCATION_LOC1,	/* Location */
	{gpioPortD, 0},
	{gpioPortD, 1},
	{
	 NULL, NULL,
	 0, 0, 0, 0,
	 },
	{0},
	NULL,
};

static EFM32_USART_RESOURCES USART1_Resources = {
	{
	 1,			/* supports UART (Asynchronous) mode */
	 0,			/* supports Synchronous Master mode */
	 0,			/* supports Synchronous Slave mode */
	 0,			/* supports UART Single-wire mode */
	 0,			/* supports UART IrDA mode */
	 0,			/* supports UART Smart Card mode */
	 0,			/* Smart Card Clock generator available */
	 0,			/* RTS Flow Control available */
	 0,			/* CTS Flow Control available */
	 0,			/* Transmit completed event: \ref ARM_USART_EVENT_TX_COMPLETE */
	 0,			/* Signal receive character timeout event: \ref ARM_USART_EVENT_RX_TIMEOUT */
	 0,			/* RTS Line: 0=not available, 1=available */
	 0,			/* CTS Line: 0=not available, 1=available */
	 0,			/* DTR Line: 0=not available, 1=available */
	 0,			/* DSR Line: 0=not available, 1=available */
	 0,			/* DCD Line: 0=not available, 1=available */
	 0,			/* RI Line: 0=not available, 1=available */
	 0,			/* Signal CTS change event: \ref ARM_USART_EVENT_CTS */
	 0,			/* Signal DSR change event: \ref ARM_USART_EVENT_DSR */
	 0,			/* Signal DCD change event: \ref ARM_USART_EVENT_DCD */
	 0,			/* Signal RI change event: \ref ARM_USART_EVENT_RI */
	 0
	 /* Reserved (must be zero) */
	 },
	USART1,
	USART_INITASYNC_DEFAULT,
	USART_ROUTE_LOCATION_LOC1,	/* Location */
	{gpioPortD, 0},
	{gpioPortD, 1},
	{
	 NULL, NULL,
	 0, 0, 0, 0,
	 },
	{0},
	NULL,
};

static EFM32_USART_RESOURCES LEUART0_Resources = {
	{
	 1,			/* supports UART (Asynchronous) mode */
	 0,			/* supports Synchronous Master mode */
	 0,			/* supports Synchronous Slave mode */
	 0,			/* supports UART Single-wire mode */
	 0,			/* supports UART IrDA mode */
	 0,			/* supports UART Smart Card mode */
	 0,			/* Smart Card Clock generator available */
	 0,			/* RTS Flow Control available */
	 0,			/* CTS Flow Control available */
	 0,			/* Transmit completed event: \ref ARM_USART_EVENT_TX_COMPLETE */
	 0,			/* Signal receive character timeout event: \ref ARM_USART_EVENT_RX_TIMEOUT */
	 0,			/* RTS Line: 0=not available, 1=available */
	 0,			/* CTS Line: 0=not available, 1=available */
	 0,			/* DTR Line: 0=not available, 1=available */
	 0,			/* DSR Line: 0=not available, 1=available */
	 0,			/* DCD Line: 0=not available, 1=available */
	 0,			/* RI Line: 0=not available, 1=available */
	 0,			/* Signal CTS change event: \ref ARM_USART_EVENT_CTS */
	 0,			/* Signal DSR change event: \ref ARM_USART_EVENT_DSR */
	 0,			/* Signal DCD change event: \ref ARM_USART_EVENT_DCD */
	 0,			/* Signal RI change event: \ref ARM_USART_EVENT_RI */
	 0
	 /* Reserved (must be zero) */
	 },
	LEUART0,
	LEUART_INIT_DEFAULT,
	LEUART_ROUTE_LOCATION_LOC0,	/* Location */
	{gpioPortD, 4},		// Tx
	{gpioPortD, 5},		// Rx
	{
	 NULL, NULL,
	 0, 0, 0, 0,
	 },
	{0},
	NULL,
};

// EFM32 functions
static int32_t EFM32_USART_Initialize(ARM_USART_SignalEvent_t cb_event, EFM32_USART_RESOURCES * usart)
{
	CMU_ClockEnable(cmuClock_GPIO, true);
	GPIO_PinModeSet(usart->TxPin.port, usart->TxPin.pin, gpioModePushPull, 1);	/* TX Pin */
	GPIO_PinModeSet(usart->RxPin.port, usart->RxPin.pin, gpioModeInputPull, 1);	/* RX Pin */

	/* Baudrate set to CMSIS default value */
	usart->usart_cfg.baudrate = 9600;

	if (usart->device == USART0) {
		CMU_ClockEnable(cmuClock_USART0, true);

		USART_IntEnable(USART0, USART_IEN_TXC);

		NVIC_ClearPendingIRQ(USART0_TX_IRQn);
		NVIC_EnableIRQ(USART0_TX_IRQn);
	} else if (usart->device == USART1) {
		CMU_ClockEnable(cmuClock_USART1, true);

		USART_IntEnable(USART1, USART_IEN_TXC);

		NVIC_ClearPendingIRQ(USART1_TX_IRQn);
		NVIC_EnableIRQ(USART1_TX_IRQn);
	}

	usart->xfer.TxCnt = 0;
	usart->xfer.RxCnt = 0;

	if (cb_event != NULL) {
		usart->cb_event = cb_event;
	}

	return ARM_DRIVER_OK;
}

static int32_t EFM32_LEUART_Initialize(ARM_USART_SignalEvent_t cb_event, EFM32_USART_RESOURCES * usart)
{
	CMU_ClockEnable(cmuClock_GPIO, true);
	CMU_ClockEnable(cmuClock_CORELE, true);

	GPIO_PinModeSet(usart->TxPin.port, usart->TxPin.pin, gpioModePushPull, 1);	/* TX Pin */
	GPIO_PinModeSet(usart->RxPin.port, usart->RxPin.pin, gpioModeInputPull, 1);	/* RX Pin */

#ifdef LEUART0
	if (usart->device == LEUART0) {
		CMU_ClockEnable(cmuClock_LEUART0, true);
		LEUART_IntEnable(LEUART0, LEUART_IEN_TXC);
		NVIC_ClearPendingIRQ(LEUART0_IRQn);
		NVIC_EnableIRQ(LEUART0_IRQn);
	}
#endif
#ifdef LEUART1
	if (usart->device == LEUART1) {
		CMU_ClockEnable(cmuClock_LEUART1, true);
		LEUART_IntEnable(LEUART1, LEUART_IEN_TXC);
		NVIC_ClearPendingIRQ(LEUART1_IRQn);
		NVIC_EnableIRQ(LEUART1_IRQn);
	}
#endif

	usart->xfer.TxCnt = 0;
	usart->xfer.RxCnt = 0;

	if (cb_event != NULL) {
		usart->cb_event = cb_event;
	}

	return ARM_DRIVER_OK;
}

static int32_t EFM32_USART_Uninitialize(EFM32_USART_RESOURCES * usart)
{
	if (usart->device == USART0) {
		CMU_ClockEnable(cmuClock_USART0, false);
		NVIC_DisableIRQ(USART0_TX_IRQn);
	} else if (usart->device == USART1) {
		CMU_ClockEnable(cmuClock_USART1, false);
		NVIC_DisableIRQ(USART1_TX_IRQn);
	}

	GPIO_PinModeSet(usart->TxPin.port, usart->TxPin.pin, gpioModeDisabled, 1);	/* TX Pin */
	GPIO_PinModeSet(usart->RxPin.port, usart->RxPin.pin, gpioModeDisabled, 1);	/* RX Pin */

	usart->cb_event = NULL;

	return ARM_DRIVER_OK;
}

static int32_t EFM32_LEUART_Uninitialize(EFM32_USART_RESOURCES * usart)
{
#ifdef LEUART0
	if (usart->device == LEUART0) {
		CMU_ClockEnable(cmuClock_LEUART0, false);
		NVIC_DisableIRQ(LEUART0_IRQn);
	}
#endif
#ifdef LEUART1
	if (usart->device == LEUART1) {
		CMU_ClockEnable(cmuClock_LEUART1, false);
		NVIC_DisableIRQ(LEUART1_IRQn);
	}
#endif

	GPIO_PinModeSet(usart->TxPin.port, usart->TxPin.pin, gpioModeDisabled, 1);	/* TX Pin */
	GPIO_PinModeSet(usart->RxPin.port, usart->RxPin.pin, gpioModeDisabled, 1);	/* RX Pin */

	usart->cb_event = NULL;

	return ARM_DRIVER_OK;
}

static int32_t EFM32_USART_PowerControl(ARM_POWER_STATE state, EFM32_USART_RESOURCES const *usart)
{
	if (usart->device == NULL) {
		return ARM_DRIVER_ERROR_PARAMETER;
	}

	switch (state) {
	case ARM_POWER_OFF:
		USART_IntDisable(usart->device, USART_IEN_TXC);
		USART_Enable(usart->device, false);
		break;
	case ARM_POWER_LOW:
		return ARM_DRIVER_ERROR_UNSUPPORTED;
		break;
	case ARM_POWER_FULL:
		/* Enable TX Complete IRQ */
		USART_IntEnable(usart->device, USART_IEN_TXC);
		USART_Enable(usart->device, true);
		break;
	}

	return ARM_DRIVER_OK;
}

static int32_t EFM32_LEUART_PowerControl(ARM_POWER_STATE state, EFM32_USART_RESOURCES const *usart)
{
	if (usart->device == NULL) {
		return ARM_DRIVER_ERROR_PARAMETER;
	}

	switch (state) {
	case ARM_POWER_OFF:
		LEUART_IntDisable(usart->device, LEUART_IEN_TXC);
		LEUART_Enable(usart->device, false);
		break;
	case ARM_POWER_LOW:
		return ARM_DRIVER_ERROR_UNSUPPORTED;
		break;
	case ARM_POWER_FULL:
		LEUART_IntEnable(usart->device, LEUART_IEN_TXC);
		LEUART_Enable(usart->device, true);
		break;
	}

	return ARM_DRIVER_OK;
}

static int32_t EFM32_USART_Send(const void *data, uint32_t num, EFM32_USART_RESOURCES * usart)
{
	if (usart->device == NULL) {
		return ARM_DRIVER_ERROR_PARAMETER;
	}

	if ((data == NULL) || (num == 0U)) {
		return ARM_DRIVER_ERROR_PARAMETER;
	}

	// Using interrupts
	char *aux = (char *)data;
	usart->xfer.TxBuf = (void *)data;
	usart->xfer.TxNum = num;
	usart->xfer.TxCnt = 0;
	USART_Tx(usart->device, *aux);	// This will trigger the TX Done IRQ

	return ARM_DRIVER_OK;
}

static int32_t EFM32_LEUART_Send(const void *data, uint32_t num, EFM32_USART_RESOURCES * usart)
{
	if (usart->device == NULL) {
		return ARM_DRIVER_ERROR_PARAMETER;
	}

	if ((data == NULL) || (num == 0U)) {
		return ARM_DRIVER_ERROR_PARAMETER;
	}

	// Using interrupts
	char *aux = (char *)data;
	usart->xfer.TxBuf = (void *)data;
	usart->xfer.TxNum = num;
	usart->xfer.TxCnt = 0;
	LEUART_Tx(usart->device, *aux++);

	return ARM_DRIVER_OK;
}

static int32_t EFM32_USART_Receive(const void *data, uint32_t num, EFM32_USART_RESOURCES * usart)
{
	if (usart->device == NULL) {
		return ARM_DRIVER_ERROR_PARAMETER;
	}

	if ((data == NULL) || (num == 0U)) {
		return ARM_DRIVER_ERROR_PARAMETER;
	}

	if (usart->status.rx_busy == true) {
		return ARM_DRIVER_ERROR_BUSY;
	}

	// Use interrupts
	usart->xfer.RxBuf = (void *)data;
	usart->xfer.RxNum = num;
	usart->xfer.RxCnt = 0;
	usart->status.rx_busy = true;
	USART_IntEnable(usart->device, USART_IEN_RXDATAV);

#ifdef USART0
	if (usart->device == USART0) {
		NVIC_EnableIRQ(USART0_RX_IRQn);
	}
#endif
#ifdef USART1
	if (usart->device == USART1) {
		NVIC_EnableIRQ(USART1_RX_IRQn);
	}
#endif
#ifdef USART2
	if (usart->device == USART2) {
		NVIC_EnableIRQ(USART2_RX_IRQn);
	}
#endif

	return ARM_DRIVER_OK;
}

static int32_t EFM32_LEUART_Receive(const void *data, uint32_t num, EFM32_USART_RESOURCES * usart)
{
	if (usart->device == NULL) {
		return ARM_DRIVER_ERROR_PARAMETER;
	}

	if ((data == NULL) || (num == 0U)) {
		return ARM_DRIVER_ERROR_PARAMETER;
	}

	if (usart->status.rx_busy == true) {
		return ARM_DRIVER_ERROR_BUSY;
	}

	// Use interrupts
	usart->xfer.RxBuf = (void *)data;
	usart->xfer.RxNum = num;
	usart->xfer.RxCnt = 0;
	usart->status.rx_busy = true;
	LEUART_IntEnable(usart->device, LEUART_IEN_RXDATAV);

#ifdef LEUART0
	if (usart->device == LEUART0) {
		NVIC_EnableIRQ(LEUART0_IRQn);
	}
#endif
#ifdef LEUART1
	if (usart->device == LEUART1) {
		NVIC_EnableIRQ(LEUART1_IRQn);
	}
#endif

	return ARM_DRIVER_OK;
}

static int32_t EFM32_USART_Transfer(const void *data_out, void *data_in,
				    uint32_t num, EFM32_USART_RESOURCES const *usart)
{
	return ARM_DRIVER_ERROR_UNSUPPORTED;
}

static int32_t EFM32_LEUART_Transfer(const void *data_out, void *data_in,
				     uint32_t num, EFM32_USART_RESOURCES const *usart)
{
	return ARM_DRIVER_ERROR_UNSUPPORTED;
}

static uint32_t EFM32_USART_GetTxCount(EFM32_USART_RESOURCES const *usart)
{
	return usart->xfer.TxCnt;
}

static uint32_t EFM32_LEUART_GetTxCount(EFM32_USART_RESOURCES const *usart)
{
	return usart->xfer.TxCnt;
}

static uint32_t EFM32_USART_GetRxCount(EFM32_USART_RESOURCES const *usart)
{
	return usart->xfer.RxCnt;
}

static uint32_t EFM32_LEUART_GetRxCount(EFM32_USART_RESOURCES const *usart)
{
	return usart->xfer.RxCnt;
}

static int32_t EFM32_USART_Control(uint32_t control, uint32_t arg, EFM32_USART_RESOURCES * usart)
{
	switch (control & ARM_USART_CONTROL_Msk) {
	case ARM_USART_CONTROL_TX:
		if (arg != 0) {
#ifdef USART0
			if (usart->device == USART0) {
				NVIC_EnableIRQ(USART0_TX_IRQn);
			}
#endif
#ifdef USART1
			if (usart->device == USART1) {
				NVIC_EnableIRQ(USART1_TX_IRQn);
			}
#endif
#ifdef USART2
			if (usart->device == USART2) {
				NVIC_EnableIRQ(USART2_TX_IRQn);
			}
#endif

			USART_IntEnable(usart->device, USART_IEN_TXC);
		} else {
			USART_IntDisable(usart->device, USART_IEN_TXC);
		}

		return ARM_DRIVER_OK;

	case ARM_USART_CONTROL_RX:
		if (arg != 0) {
#ifdef USART0
			if (usart->device == USART0) {
				NVIC_EnableIRQ(USART0_RX_IRQn);
			}
#endif
#ifdef USART1
			if (usart->device == USART1) {
				NVIC_EnableIRQ(USART1_RX_IRQn);
			}
#endif
#ifdef USART2
			if (usart->device == USART2) {
				NVIC_EnableIRQ(USART2_RX_IRQn);
			}
#endif

			USART_IntEnable(usart->device, USART_IEN_RXDATAV);
		} else {
			USART_IntDisable(usart->device, USART_IEN_RXDATAV);
		}

		return ARM_DRIVER_OK;

	case ARM_USART_MODE_ASYNCHRONOUS:
		usart->usart_cfg.baudrate = arg;
		break;
	default:
		return ARM_DRIVER_ERROR_PARAMETER;
	}

	switch (control & ARM_USART_DATA_BITS_Msk) {
	case ARM_USART_DATA_BITS_5:
	case ARM_USART_DATA_BITS_6:
	case ARM_USART_DATA_BITS_7:
		return ARM_DRIVER_ERROR_PARAMETER;
	case ARM_USART_DATA_BITS_8:
		usart->usart_cfg.databits = usartDatabits8;
		break;
	case ARM_USART_DATA_BITS_9:
		usart->usart_cfg.databits = usartDatabits9;
		break;
	default:
		return ARM_DRIVER_ERROR_PARAMETER;
	}

	switch (control & ARM_USART_PARITY_Msk) {
	case ARM_USART_PARITY_NONE:
		usart->usart_cfg.parity = usartNoParity;
		break;
	case ARM_USART_PARITY_ODD:
		usart->usart_cfg.parity = usartOddParity;
		break;
	case ARM_USART_PARITY_EVEN:
		usart->usart_cfg.parity = usartEvenParity;
		break;
	default:
		return ARM_DRIVER_ERROR_PARAMETER;
	}

	switch (control & ARM_USART_STOP_BITS_Msk) {
	case ARM_USART_STOP_BITS_0_5:
		usart->usart_cfg.stopbits = usartStopbits0p5;
		break;
	case ARM_USART_STOP_BITS_1:
		usart->usart_cfg.stopbits = usartStopbits1;
		break;
	case ARM_USART_STOP_BITS_1_5:
		usart->usart_cfg.stopbits = usartStopbits1p5;
		break;
	case ARM_USART_STOP_BITS_2:
		usart->usart_cfg.stopbits = usartStopbits2;
		break;
	default:
		return ARM_DRIVER_ERROR_PARAMETER;
	}

	switch (control & ARM_USART_FLOW_CONTROL_Msk) {
	case ARM_USART_FLOW_CONTROL_NONE:
#if (_SILICON_LABS_32B_SERIES > 0)
		usart->usart_cfg.hwFlowControl = usartHwFlowControlNone;
		break;
#else
		break;
#endif
	case ARM_USART_FLOW_CONTROL_RTS:
#if (_SILICON_LABS_32B_SERIES > 0)
		usart->usart_cfg.hwFlowControl = usartHwFlowControlRts;
		break;
#else
		return ARM_DRIVER_ERROR_PARAMETER;
#endif
	case ARM_USART_FLOW_CONTROL_CTS:
#if (_SILICON_LABS_32B_SERIES > 0)
		usart->usart_cfg.hwFlowControl = usartHwFlowControlCts;
		break;
#else
		return ARM_DRIVER_ERROR_PARAMETER;
#endif
	case ARM_USART_FLOW_CONTROL_RTS_CTS:
#if (_SILICON_LABS_32B_SERIES > 0)
		usart->usart_cfg.hwFlowControl = usartHwFlowControlCtsAndRts;
		break;
#else
		return ARM_DRIVER_ERROR_PARAMETER;
#endif
	default:
		return ARM_DRIVER_ERROR_PARAMETER;
	}

	USART_InitAsync(usart->device, &usart->usart_cfg);

	((USART_TypeDef *) usart->device)->ROUTE = USART_ROUTE_RXPEN | USART_ROUTE_TXPEN | usart->LOCATION;

	return ARM_DRIVER_OK;
}

static int32_t EFM32_LEUART_Control(uint32_t control, uint32_t arg, EFM32_USART_RESOURCES const *usart)
{
	LEUART_Init_TypeDef leuart_cfg = LEUART_INIT_DEFAULT;

	switch (control & ARM_USART_CONTROL_Msk) {

	case ARM_USART_CONTROL_TX:
		if (arg != 0) {
#ifdef LEUART0
			if (usart->device == LEUART0) {
				NVIC_EnableIRQ(LEUART0_IRQn);
			}
#endif
#ifdef LEUART1
			if (usart->device == LEUART1) {
				NVIC_EnableIRQ(LEUART1_IRQn);
			}
#endif

			LEUART_IntEnable(usart->device, LEUART_IEN_TXC);
		} else {
			LEUART_IntDisable(usart->device, LEUART_IEN_TXC);
		}

		return ARM_DRIVER_OK;

	case ARM_USART_CONTROL_RX:
		if (arg != 0) {
#ifdef LEUART0
			if (usart->device == LEUART0) {
				NVIC_EnableIRQ(LEUART0_IRQn);
			}
#endif
#ifdef LEUART1
			if (usart->device == LEUART1) {
				NVIC_EnableIRQ(LEUART1_IRQn);
			}
#endif
			LEUART_IntEnable(usart->device, LEUART_IEN_RXDATAV);
		} else {
			LEUART_IntDisable(usart->device, LEUART_IEN_RXDATAV);
		}

		return ARM_DRIVER_OK;

	case ARM_USART_MODE_ASYNCHRONOUS:
		leuart_cfg.baudrate = arg;
		break;
	default:
		return ARM_DRIVER_ERROR_PARAMETER;
	}

	switch (control & ARM_USART_DATA_BITS_Msk) {
	case ARM_USART_DATA_BITS_5:
	case ARM_USART_DATA_BITS_6:
	case ARM_USART_DATA_BITS_7:
		return ARM_DRIVER_ERROR_PARAMETER;
	case ARM_USART_DATA_BITS_8:
		leuart_cfg.databits = leuartDatabits8;
		break;
	case ARM_USART_DATA_BITS_9:
		leuart_cfg.databits = leuartDatabits9;
		break;
	default:
		return ARM_DRIVER_ERROR_PARAMETER;
	}

	switch (control & ARM_USART_PARITY_Msk) {
	case ARM_USART_PARITY_NONE:
		leuart_cfg.parity = leuartNoParity;
		break;
	case ARM_USART_PARITY_ODD:
		leuart_cfg.parity = leuartOddParity;
		break;
	case ARM_USART_PARITY_EVEN:
		leuart_cfg.parity = leuartEvenParity;
		break;
	default:
		return ARM_DRIVER_ERROR_PARAMETER;
	}

	switch (control & ARM_USART_STOP_BITS_Msk) {
	case ARM_USART_STOP_BITS_0_5:
		return ARM_DRIVER_ERROR_PARAMETER;
		break;
	case ARM_USART_STOP_BITS_1:
		leuart_cfg.stopbits = leuartStopbits1;
		break;
	case ARM_USART_STOP_BITS_1_5:
		return ARM_DRIVER_ERROR_PARAMETER;
		break;
	case ARM_USART_STOP_BITS_2:
		leuart_cfg.stopbits = leuartStopbits2;
		break;
	default:
		return ARM_DRIVER_ERROR_PARAMETER;
	}

	switch (control & ARM_USART_FLOW_CONTROL_Msk) {
	case ARM_USART_FLOW_CONTROL_NONE:
		break;
	case ARM_USART_FLOW_CONTROL_RTS:
	case ARM_USART_FLOW_CONTROL_CTS:
	case ARM_USART_FLOW_CONTROL_RTS_CTS:
		return ARM_DRIVER_ERROR_PARAMETER;
	}

	LEUART_Init(usart->device, &leuart_cfg);
	((LEUART_TypeDef *) usart->device)->ROUTE = LEUART_ROUTE_RXPEN | LEUART_ROUTE_TXPEN | usart->LOCATION;

	return ARM_DRIVER_OK;
}

static ARM_USART_STATUS EFM32_USART_GetStatus(EFM32_USART_RESOURCES const
					      *usart)
{
	ARM_USART_STATUS status = {
		0
	};
	return status;
}

static ARM_USART_STATUS EFM32_LEUART_GetStatus(EFM32_USART_RESOURCES const
					       *usart)
{
	ARM_USART_STATUS status = {
		0
	};
	return status;
}

static int32_t EFM32_USART_SetModemControl(ARM_USART_MODEM_CONTROL control, EFM32_USART_RESOURCES const *usart)
{
	return 0;
}

static int32_t EFM32_LEUART_SetModemControl(ARM_USART_MODEM_CONTROL control, EFM32_USART_RESOURCES const *usart)
{
	return 0;
}

static ARM_USART_MODEM_STATUS EFM32_USART_GetModemStatus(EFM32_USART_RESOURCES const *usart)
{
	ARM_USART_MODEM_STATUS status = {
		0
	};
	return status;
}

static ARM_USART_MODEM_STATUS EFM32_LEUART_GetModemStatus(EFM32_USART_RESOURCES const *usart)
{
	ARM_USART_MODEM_STATUS status = {
		0
	};
	return status;
}

void USART_TX_IRQHandler(EFM32_USART_RESOURCES * usart)
{
	uint32_t flags;

	flags = USART_IntGet(usart->device);
	USART_IntClear(usart->device, flags);

	if (flags & USART_IF_TXC) {
		usart->xfer.TxCnt++;
		/* Continue sending TX Buffer */
		if (usart->xfer.TxCnt < usart->xfer.TxNum) {
			char *aux = (char *)usart->xfer.TxBuf;
			USART_Tx(usart->device, aux[usart->xfer.TxCnt]);
		}
	}
}

void USART_RX_IRQHandler(EFM32_USART_RESOURCES * usart)
{
	uint32_t flags;
	uint32_t event = 0;

	flags = USART_IntGet(usart->device);
	USART_IntClear(usart->device, flags);

	if (flags & USART_IF_RXDATAV) {
		char recv = USART_Rx(usart->device);
		if (usart->xfer.RxCnt < usart->xfer.RxNum) {
			char *aux = (char *)usart->xfer.RxBuf;
			aux[usart->xfer.RxCnt] = recv;
			usart->xfer.RxCnt++;
		}

		if (usart->xfer.RxCnt == usart->xfer.RxNum) {
			event = ARM_USART_EVENT_RECEIVE_COMPLETE;
			usart->status.rx_busy = false;
			USART_IntDisable(usart->device, USART_IEN_RXDATAV);	// Disable Rx
		}
	}

	if ((event != 0) && (usart->cb_event != NULL)) {
		usart->cb_event(event);
	}
}

void LEUART_TX_IRQHandler(EFM32_USART_RESOURCES * usart)
{
	uint32_t flags;

	flags = LEUART_IntGet(usart->device);
	LEUART_IntClear(usart->device, flags);

	if (flags & LEUART_IF_TXC) {
		usart->xfer.TxCnt++;
		/* Continue sending TX Buffer */
		if (usart->xfer.TxCnt < usart->xfer.TxNum) {
			char *aux = (char *)usart->xfer.TxBuf;
			LEUART_Tx(usart->device, aux[usart->xfer.TxCnt]);
		}
	}
}

void LEUART_RX_IRQHandler(EFM32_USART_RESOURCES * usart)
{
	uint32_t flags;
	uint32_t event = 0;

	flags = LEUART_IntGet(usart->device);
	LEUART_IntClear(usart->device, flags);

	if (flags & LEUART_IF_RXDATAV) {
		char recv = LEUART_Rx(usart->device);
		if (usart->xfer.RxCnt < usart->xfer.RxNum) {
			char *aux = (char *)usart->xfer.RxBuf;
			aux[usart->xfer.RxCnt] = recv;
			usart->xfer.RxCnt++;
		}

		if (usart->xfer.RxCnt == usart->xfer.RxNum) {
			event = ARM_USART_EVENT_RECEIVE_COMPLETE;
			usart->status.rx_busy = false;
			LEUART_IntDisable(usart->device, LEUART_IEN_RXDATAV);	// Disable Rx
		}
	}

	if ((event != 0) && (usart->cb_event != NULL)) {
		usart->cb_event(event);
	}
}

//
//   Functions
//

static ARM_DRIVER_VERSION ARM_GetVersion(void)
{
	return DriverVersion;
}

static ARM_USART_CAPABILITIES USART0_GetCapabilities(void)
{
	return USART0_Resources.capabilities;
}

static int32_t USART0_Initialize(ARM_USART_SignalEvent_t cb_event)
{
	return EFM32_USART_Initialize(cb_event, &USART0_Resources);
}

static int32_t USART0_Uninitialize(void)
{
	return EFM32_USART_Uninitialize(&USART0_Resources);
}

static int32_t USART0_PowerControl(ARM_POWER_STATE state)
{
	return EFM32_USART_PowerControl(state, &USART0_Resources);
}

static int32_t USART0_Send(const void *data, uint32_t num)
{
	return EFM32_USART_Send(data, num, &USART0_Resources);
}

static int32_t USART0_Receive(void *data, uint32_t num)
{
	return EFM32_USART_Receive(data, num, &USART0_Resources);
}

static int32_t USART0_Transfer(const void *data_out, void *data_in, uint32_t num)
{
	return EFM32_USART_Transfer(data_out, data_in, num, &USART0_Resources);
}

static uint32_t USART0_GetTxCount(void)
{
	return EFM32_USART_GetTxCount(&USART0_Resources);
}

static uint32_t USART0_GetRxCount(void)
{
	return EFM32_USART_GetRxCount(&USART0_Resources);
}

static int32_t USART0_Control(uint32_t control, uint32_t arg)
{
	return EFM32_USART_Control(control, arg, &USART0_Resources);
}

static ARM_USART_STATUS USART0_GetStatus(void)
{
	return EFM32_USART_GetStatus(&USART0_Resources);
}

static int32_t USART0_SetModemControl(ARM_USART_MODEM_CONTROL control)
{
	return EFM32_USART_SetModemControl(control, &USART0_Resources);
}

static ARM_USART_MODEM_STATUS USART0_GetModemStatus(void)
{
	return EFM32_USART_GetModemStatus(&USART0_Resources);
}

void USART0_TX_IRQHandler(void)
{
	USART_TX_IRQHandler(&USART0_Resources);
}

void USART0_RX_IRQHandler(void)
{
	USART_RX_IRQHandler(&USART1_Resources);
}

// USART1
static ARM_USART_CAPABILITIES USART1_GetCapabilities(void)
{
	return USART1_Resources.capabilities;
}

static int32_t USART1_Initialize(ARM_USART_SignalEvent_t cb_event)
{
	return EFM32_USART_Initialize(cb_event, &USART1_Resources);
}

static int32_t USART1_Uninitialize(void)
{
	return EFM32_USART_Uninitialize(&USART1_Resources);
}

static int32_t USART1_PowerControl(ARM_POWER_STATE state)
{
	return EFM32_USART_PowerControl(state, &USART1_Resources);
}

static int32_t USART1_Send(const void *data, uint32_t num)
{
	return EFM32_USART_Send(data, num, &USART1_Resources);
}

static int32_t USART1_Receive(void *data, uint32_t num)
{
	return EFM32_USART_Receive(data, num, &USART1_Resources);
}

static int32_t USART1_Transfer(const void *data_out, void *data_in, uint32_t num)
{
	return EFM32_USART_Transfer(data_out, data_in, num, &USART1_Resources);
}

static uint32_t USART1_GetTxCount(void)
{
	return EFM32_USART_GetTxCount(&USART1_Resources);
}

static uint32_t USART1_GetRxCount(void)
{
	return EFM32_USART_GetRxCount(&USART1_Resources);
}

static int32_t USART1_Control(uint32_t control, uint32_t arg)
{
	return EFM32_USART_Control(control, arg, &USART1_Resources);
}

static ARM_USART_STATUS USART1_GetStatus(void)
{
	return EFM32_USART_GetStatus(&USART1_Resources);
}

static int32_t USART1_SetModemControl(ARM_USART_MODEM_CONTROL control)
{
	return EFM32_USART_SetModemControl(control, &USART1_Resources);
}

static ARM_USART_MODEM_STATUS USART1_GetModemStatus(void)
{
	return EFM32_USART_GetModemStatus(&USART1_Resources);
}

void USART1_TX_IRQHandler(void)
{
	USART_TX_IRQHandler(&USART1_Resources);
}

void USART1_RX_IRQHandler(void)
{
	USART_RX_IRQHandler(&USART1_Resources);
}

// LEUART0
static ARM_USART_CAPABILITIES LEUART0_GetCapabilities(void)
{
	return LEUART0_Resources.capabilities;
}

static int32_t LEUART0_Initialize(ARM_USART_SignalEvent_t cb_event)
{
	return EFM32_LEUART_Initialize(cb_event, &LEUART0_Resources);
}

static int32_t LEUART0_Uninitialize(void)
{
	return EFM32_LEUART_Uninitialize(&LEUART0_Resources);
}

static int32_t LEUART0_PowerControl(ARM_POWER_STATE state)
{
	return EFM32_LEUART_PowerControl(state, &LEUART0_Resources);
}

static int32_t LEUART0_Send(const void *data, uint32_t num)
{
	return EFM32_LEUART_Send(data, num, &LEUART0_Resources);
}

static int32_t LEUART0_Receive(void *data, uint32_t num)
{
	return EFM32_LEUART_Receive(data, num, &LEUART0_Resources);
}

static int32_t LEUART0_Transfer(const void *data_out, void *data_in, uint32_t num)
{
	return EFM32_LEUART_Transfer(data_out, data_in, num, &LEUART0_Resources);
}

static uint32_t LEUART0_GetTxCount(void)
{
	return EFM32_LEUART_GetTxCount(&LEUART0_Resources);
}

static uint32_t LEUART0_GetRxCount(void)
{
	return EFM32_LEUART_GetRxCount(&LEUART0_Resources);
}

static int32_t LEUART0_Control(uint32_t control, uint32_t arg)
{
	return EFM32_LEUART_Control(control, arg, &LEUART0_Resources);
}

static ARM_USART_STATUS LEUART0_GetStatus(void)
{
	return EFM32_LEUART_GetStatus(&LEUART0_Resources);
}

static int32_t LEUART0_SetModemControl(ARM_USART_MODEM_CONTROL control)
{
	return EFM32_LEUART_SetModemControl(control, &LEUART0_Resources);
}

static ARM_USART_MODEM_STATUS LEUART0_GetModemStatus(void)
{
	return EFM32_LEUART_GetModemStatus(&LEUART0_Resources);
}

void LEUART0_IRQHandler(void)
{
	uint32_t flags;
	flags = LEUART_IntGet(LEUART0);

	if (flags & LEUART_IF_TXC) {
		LEUART_TX_IRQHandler(&LEUART0_Resources);
	} else if (flags & LEUART_IF_RXDATAV) {
		LEUART_RX_IRQHandler(&LEUART0_Resources);
	}
}

ARM_DRIVER_USART Driver_USART0 = {
	ARM_GetVersion,
	USART0_GetCapabilities,
	USART0_Initialize,
	USART0_Uninitialize,
	USART0_PowerControl,
	USART0_Send,
	USART0_Receive,
	USART0_Transfer,
	USART0_GetTxCount,
	USART0_GetRxCount,
	USART0_Control,
	USART0_GetStatus,
	USART0_SetModemControl,
	USART0_GetModemStatus
};

ARM_DRIVER_USART Driver_LEUART0 = {
	ARM_GetVersion,
	LEUART0_GetCapabilities,
	LEUART0_Initialize,
	LEUART0_Uninitialize,
	LEUART0_PowerControl,
	LEUART0_Send,
	LEUART0_Receive,
	LEUART0_Transfer,
	LEUART0_GetTxCount,
	LEUART0_GetRxCount,
	LEUART0_Control,
	LEUART0_GetStatus,
	LEUART0_SetModemControl,
	LEUART0_GetModemStatus,
};

ARM_DRIVER_USART Driver_USART1 = {
	ARM_GetVersion,
	USART1_GetCapabilities,
	USART1_Initialize,
	USART1_Uninitialize,
	USART1_PowerControl,
	USART1_Send,
	USART1_Receive,
	USART1_Transfer,
	USART1_GetTxCount,
	USART1_GetRxCount,
	USART1_Control,
	USART1_GetStatus,
	USART1_SetModemControl,
	USART1_GetModemStatus
};
