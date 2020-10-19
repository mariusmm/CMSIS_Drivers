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
 * This example implements a MODBUS client using CMSIS UART Driver
 */

#include <stdbool.h>
#include "Driver_USART.h"

/**
 * Initializes MODBUS client
 * @param driver_usart CMSIS UART driver to use
 * @return true on success, false otherwise
 */
bool MODBUS_Init(ARM_DRIVER_USART * driver_usart);

/**
 * Waits for a MODBUS request and process it
 * @param timeout time to wait until return
 * @return true on success, false otherwise
 */
bool do_MODBUS_Client(uint32_t timeout);
