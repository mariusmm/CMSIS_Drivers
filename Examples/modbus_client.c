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
 * This example implements a MODBUS client using CMSIS UART Driver. It implements
 * holding register functions (3, 6 & 16), with a maximum pkt length of 263 bytes,
 * corresponding a function 16 with 254 bytes to write (127 registers + 9 bytes).
 *
 */

#include <stdbool.h>
#include "cmsis_compiler.h"
#include "Driver_USART.h"
#include "modbus_client.h"

#define MAX_RECV_BUFF (254+9)

#define READ_HOLDING_REGS (3)
#define WRITE_SINGLE_REG (6)
#define WRITE_MULTS_REGS (16)

static ARM_DRIVER_USART *usart_drv;
static uint8_t recv_buf[MAX_RECV_BUFF];
static uint8_t send_buf[15];

static volatile bool data_received = false;

typedef struct {
    uint8_t address;
    uint8_t function;
    uint8_t start_addr_hi;
    uint8_t start_addr_lo;
    uint8_t regs_num_hi;
    uint8_t regs_num_lo;
    uint8_t crc_lo;
    uint8_t crc_hi;
} __attribute__((packed)) read_holding_register_t;

typedef struct {
    uint8_t address;
    uint8_t function;
    uint8_t byte_count;
    uint8_t data[];
} __attribute__((packed)) read_holding_register_response_t;

typedef struct {
    uint8_t address;
    uint8_t function;
    uint8_t start_addr_hi;
    uint8_t start_addr_lo;
    uint8_t wr_data_hi;
    uint8_t wr_data_lo;
    uint8_t crc_lo;
    uint8_t crc_hi;
} __attribute__((packed)) write_single_register_t;

typedef struct {
    uint8_t address;
    uint8_t function;
    uint8_t start_addr_hi;
    uint8_t start_addr_lo;
    uint8_t regs_num_hi;
    uint8_t regs_num_lo;
    uint8_t byte_count;
    uint8_t data[];
} __attribute__((packed)) write_multiple_register_t;

typedef struct {
    uint8_t address;
    uint8_t function;
    uint8_t start_addr_hi;
    uint8_t start_addr_lo;
    uint8_t regs_num_hi;
    uint8_t regs_num_lo;
    uint8_t crc_lo;
    uint8_t crc_hi;
} __attribute__((packed)) write_multiple_register_response_t;

/* Extern functions */
extern uint16_t read_register(uint16_t addr);
extern void write_register(uint16_t addr, uint16_t data);
extern uint32_t getSysTicks(void);

/* Forward declarations */
void usart_event(uint32_t event);
bool process_function_3(uint8_t * buff);
bool process_function_6(uint8_t * buff);
bool process_function_16(uint8_t * buff);

/* Function (c) 2020 by Witte Software (https://www.modbustools.com/modbus.html#crc) */
uint16_t CRC16(const uint8_t * nData, uint16_t wLength);

bool MODBUS_Init(ARM_DRIVER_USART * driver_usart)
{
    bool ret_val = true;

    if (driver_usart != NULL) {
        usart_drv = driver_usart;
    } else {
        return false;
    }

    if (usart_drv->Initialize(usart_event) != ARM_DRIVER_OK) {
        ret_val = false;
    }

    if (usart_drv->Control(ARM_USART_MODE_ASYNCHRONOUS |
                           ARM_USART_DATA_BITS_8 |
                           ARM_USART_PARITY_NONE | ARM_USART_STOP_BITS_1 | ARM_USART_FLOW_CONTROL_NONE, 9600)
        != ARM_DRIVER_OK) {
        ret_val = false;
    }

    if (usart_drv->Control(ARM_USART_CONTROL_TX, 1) != ARM_DRIVER_OK) {
        ret_val = false;
    }

    if (usart_drv->Control(ARM_USART_CONTROL_RX, 1) != ARM_DRIVER_OK) {
        ret_val = false;
    }

    return ret_val;
}

bool do_MODBUS_Client(uint32_t timeout)
{

    uint32_t ticks_now;
    uint32_t pending_bytes;
    uint8_t function;

    /* Try to receive first 6 bytes, a MODBUS pkt is always larger than that */
    usart_drv->Receive(recv_buf, 6);

    /* Wait for 'timeout' time, if nothing received, return, otherwise, continue */

    ticks_now = getSysTicks();
    data_received = false;
    while ((data_received == false) && ((getSysTicks() - ticks_now) < timeout)) {
        __WFE();
    }

    if (data_received == false) {
        return false;
    }

    function = recv_buf[1];

    /* Check what function is required */
    switch (function) {
    case READ_HOLDING_REGS:
    case WRITE_SINGLE_REG:
        pending_bytes = 0;
        break;
    case WRITE_MULTS_REGS:{
            write_multiple_register_t *aux = (write_multiple_register_t *) recv_buf;
            pending_bytes = 1 + ((aux->regs_num_hi << 8) | aux->regs_num_lo) * 2;
            break;
        }
    default:
        return false;
    }

    /* Continue receiving */
    pending_bytes += 2;         /* take into account the CRC */

    usart_drv->Receive(&recv_buf[6], pending_bytes);

    ticks_now = getSysTicks();
    data_received = false;
    while ((data_received == false) && ((getSysTicks() - ticks_now) < timeout)) {
        __WFE();
    }

    if (data_received == false) {
        return false;
    }

    /* Entire packet received, process it */
    bool ret_val;
    switch (function) {
    case READ_HOLDING_REGS:
        ret_val = process_function_3(recv_buf);
        break;
    case WRITE_SINGLE_REG:
        ret_val = process_function_6(recv_buf);
        break;
    case WRITE_MULTS_REGS:
        ret_val = process_function_16(recv_buf);
        break;
    default:
        ret_val = false;
    }

    return ret_val;
}

void usart_event(uint32_t event)
{

    if (event == ARM_USART_EVENT_RECEIVE_COMPLETE) {
        data_received = true;
    }
}

bool process_function_3(uint8_t * buff)
{
    uint16_t num_registers;
    uint16_t addr_start;
    int i;

    read_holding_register_t *pkt = (read_holding_register_t *) buff;
    read_holding_register_response_t *pkt_resp = (read_holding_register_response_t *) send_buf;

    /* Check CRC */
    uint16_t crc = CRC16((uint8_t *) pkt, 6);
    uint16_t pkt_crc = (pkt->crc_hi << 8) | pkt->crc_lo;
    if (crc != pkt_crc) {
        return false;
    }

    pkt_resp->address = pkt->address;
    pkt_resp->function = pkt->function;
    num_registers = (pkt->regs_num_hi << 8) | pkt->regs_num_lo;
    addr_start = (pkt->start_addr_hi << 8) | pkt->start_addr_lo;

    pkt_resp->byte_count = num_registers * 2;
    uint16_t aux;
    for (i = 0; i < num_registers; i++) {
        aux = read_register(addr_start + i);
        pkt_resp->data[i * 2] = aux >> 8;
        pkt_resp->data[(i * 2) + 1] = aux & 0x00FF;
    }

    /* Insert CRC to the response packet */
    uint16_t crc_res_pkt = CRC16((uint8_t *) pkt_resp, 3 + (num_registers * 2));
    pkt_resp->data[i * 2] = crc_res_pkt & 0x00FF;
    pkt_resp->data[(i * 2) + 1] = crc_res_pkt >> 8;

    int32_t ret_val = usart_drv->Send(pkt_resp, 5 + (num_registers * 2));

    if (ret_val == ARM_DRIVER_OK) {
        return true;
    } else {
        return false;
    }
}

bool process_function_6(uint8_t * buff)
{
    uint16_t wr_data;
    uint16_t addr_start;
    write_single_register_t *pkt = (write_single_register_t *) buff;

    /* Check CRC */
    uint16_t crc = CRC16((uint8_t *) pkt, 6);
    uint16_t pkt_crc = (pkt->crc_hi << 8) | pkt->crc_lo;
    if (crc != pkt_crc) {
        return false;
    }

    addr_start = (pkt->start_addr_hi << 8) | pkt->start_addr_lo;
    wr_data = (pkt->wr_data_hi << 8) | pkt->wr_data_lo;

    write_register(addr_start, wr_data);

    int32_t ret_val = usart_drv->Send(buff, 8);

    if (ret_val == ARM_DRIVER_OK) {
        return true;
    } else {
        return false;
    }

}

bool process_function_16(uint8_t * buff)
{
    uint16_t addr_start;
    uint16_t num_registers;
    int i;
    uint16_t wr_data;

    write_multiple_register_t *pkt = (write_multiple_register_t *) buff;

    write_multiple_register_response_t *pkt_resp = (write_multiple_register_response_t *) send_buf;

    addr_start = (pkt->start_addr_hi << 8) | pkt->start_addr_lo;
    num_registers = (pkt->regs_num_hi << 8) | pkt->regs_num_lo;

    /* Check CRC */
    /* Variable length packet, need to calculate index CRC */
    int index = pkt->byte_count + 7;
    uint16_t crc = CRC16((uint8_t *) pkt, index);
    uint16_t pkt_crc = buff[index] | (buff[index + 1] << 8);

    if (crc != pkt_crc) {
        return false;
    }

    for (i = 0; i < num_registers; i++) {
        wr_data = (pkt->data[i * 2] << 8) | pkt->data[(i * 2) + 1];
        write_register(addr_start + i, wr_data);
    }

    pkt_resp->address = pkt->address;
    pkt_resp->function = pkt->function;
    pkt_resp->start_addr_hi = pkt->start_addr_hi;
    pkt_resp->start_addr_lo = pkt->start_addr_lo;
    pkt_resp->regs_num_hi = pkt->regs_num_hi;
    pkt_resp->regs_num_lo = pkt->regs_num_lo;

    /* Insert CRC to the response packet */
    uint16_t crc_res_pkt = CRC16((uint8_t *) pkt_resp, 6);
    pkt_resp->crc_lo = crc_res_pkt & 0x00FF;
    pkt_resp->crc_hi = crc_res_pkt >> 8;

    int32_t ret_val = usart_drv->Send(pkt_resp, 8);

    if (ret_val == ARM_DRIVER_OK) {
        return true;
    } else {
        return false;
    }
}

uint16_t CRC16(const uint8_t * nData, uint16_t wLength)
{
    static const uint16_t wCRCTable[] = {
        0X0000, 0XC0C1, 0XC181, 0X0140, 0XC301, 0X03C0, 0X0280, 0XC241,
        0XC601, 0X06C0, 0X0780, 0XC741, 0X0500, 0XC5C1, 0XC481, 0X0440,
        0XCC01, 0X0CC0, 0X0D80, 0XCD41, 0X0F00, 0XCFC1, 0XCE81, 0X0E40,
        0X0A00, 0XCAC1, 0XCB81, 0X0B40, 0XC901, 0X09C0, 0X0880, 0XC841,
        0XD801, 0X18C0, 0X1980, 0XD941, 0X1B00, 0XDBC1, 0XDA81, 0X1A40,
        0X1E00, 0XDEC1, 0XDF81, 0X1F40, 0XDD01, 0X1DC0, 0X1C80, 0XDC41,
        0X1400, 0XD4C1, 0XD581, 0X1540, 0XD701, 0X17C0, 0X1680, 0XD641,
        0XD201, 0X12C0, 0X1380, 0XD341, 0X1100, 0XD1C1, 0XD081, 0X1040,
        0XF001, 0X30C0, 0X3180, 0XF141, 0X3300, 0XF3C1, 0XF281, 0X3240,
        0X3600, 0XF6C1, 0XF781, 0X3740, 0XF501, 0X35C0, 0X3480, 0XF441,
        0X3C00, 0XFCC1, 0XFD81, 0X3D40, 0XFF01, 0X3FC0, 0X3E80, 0XFE41,
        0XFA01, 0X3AC0, 0X3B80, 0XFB41, 0X3900, 0XF9C1, 0XF881, 0X3840,
        0X2800, 0XE8C1, 0XE981, 0X2940, 0XEB01, 0X2BC0, 0X2A80, 0XEA41,
        0XEE01, 0X2EC0, 0X2F80, 0XEF41, 0X2D00, 0XEDC1, 0XEC81, 0X2C40,
        0XE401, 0X24C0, 0X2580, 0XE541, 0X2700, 0XE7C1, 0XE681, 0X2640,
        0X2200, 0XE2C1, 0XE381, 0X2340, 0XE101, 0X21C0, 0X2080, 0XE041,
        0XA001, 0X60C0, 0X6180, 0XA141, 0X6300, 0XA3C1, 0XA281, 0X6240,
        0X6600, 0XA6C1, 0XA781, 0X6740, 0XA501, 0X65C0, 0X6480, 0XA441,
        0X6C00, 0XACC1, 0XAD81, 0X6D40, 0XAF01, 0X6FC0, 0X6E80, 0XAE41,
        0XAA01, 0X6AC0, 0X6B80, 0XAB41, 0X6900, 0XA9C1, 0XA881, 0X6840,
        0X7800, 0XB8C1, 0XB981, 0X7940, 0XBB01, 0X7BC0, 0X7A80, 0XBA41,
        0XBE01, 0X7EC0, 0X7F80, 0XBF41, 0X7D00, 0XBDC1, 0XBC81, 0X7C40,
        0XB401, 0X74C0, 0X7580, 0XB541, 0X7700, 0XB7C1, 0XB681, 0X7640,
        0X7200, 0XB2C1, 0XB381, 0X7340, 0XB101, 0X71C0, 0X7080, 0XB041,
        0X5000, 0X90C1, 0X9181, 0X5140, 0X9301, 0X53C0, 0X5280, 0X9241,
        0X9601, 0X56C0, 0X5780, 0X9741, 0X5500, 0X95C1, 0X9481, 0X5440,
        0X9C01, 0X5CC0, 0X5D80, 0X9D41, 0X5F00, 0X9FC1, 0X9E81, 0X5E40,
        0X5A00, 0X9AC1, 0X9B81, 0X5B40, 0X9901, 0X59C0, 0X5880, 0X9841,
        0X8801, 0X48C0, 0X4980, 0X8941, 0X4B00, 0X8BC1, 0X8A81, 0X4A40,
        0X4E00, 0X8EC1, 0X8F81, 0X4F40, 0X8D01, 0X4DC0, 0X4C80, 0X8C41,
        0X4400, 0X84C1, 0X8581, 0X4540, 0X8701, 0X47C0, 0X4680, 0X8641,
        0X8201, 0X42C0, 0X4380, 0X8341, 0X4100, 0X81C1, 0X8081, 0X4040
    };

    uint8_t nTemp;
    uint16_t wCRCWord = 0xFFFF;

    while (wLength--) {
        nTemp = *nData++ ^ wCRCWord;
        wCRCWord >>= 8;
        wCRCWord ^= wCRCTable[nTemp];
    }
    return wCRCWord;

}
