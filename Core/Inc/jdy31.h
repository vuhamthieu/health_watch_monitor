/**
 * @file    jdy31.h
 * @brief   JDY-31 Bluetooth SPP module driver (USART1, PA9/PA10).
 *          Handles AT command setup at boot and SPP data exchange during operation.
 *
 * AT commands used at boot (module enters AT mode only when not paired):
 *   AT+NAME<name>\r\n   → set device name
 *   AT+BAUD<n>\r\n       → set baud rate (4=9600, 6=38400, 8=115200)
 *   AT+RESET\r\n         → reset module
 *
 * Data mode: raw byte stream over UART (no framing from module side).
 */

#ifndef __JDY31_H
#define __JDY31_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"
#include "app_config.h"

/* ========================================================================== *
 *  Status
 * ========================================================================== */
typedef enum {
    JDY31_OK        = 0,
    JDY31_ERR_UART  = 1,
    JDY31_ERR_TIMEOUT = 2,
} JDY31_Status_t;

/* ========================================================================== *
 *  BLE packet (what sensorTask puts in xBleQueue)
 * ========================================================================== */
typedef struct {
    uint16_t bpm;
    uint8_t  spo2;
    uint32_t steps;
    uint8_t  hh;
    uint8_t  mm;
} BlePacket_t;

/* ========================================================================== *
 *  Incoming command types (parsed from UART RX)
 * ========================================================================== */
typedef enum {
    BLE_CMD_NONE        = 0,
    BLE_CMD_SYNC_TIME,          /**< SYNC_TIME:<seconds_since_midnight>      */
    BLE_CMD_RESET_STEPS,        /**< RESET_STEPS                             */
    BLE_CMD_GET_DATA,           /**< GET_DATA                                */
    BLE_CMD_UNKNOWN,
} BleCommandType_t;

typedef struct {
    BleCommandType_t type;
    uint32_t         param;     /**< e.g. seconds for SYNC_TIME             */
} BleCommand_t;

/* ========================================================================== *
 *  API
 * ========================================================================== */

/**
 * @brief  Initialise JDY-31: configure UART, enable IT-based RX, optionally
 *         send AT setup commands if module is in AT mode.
 */
JDY31_Status_t JDY31_Init(void);

/**
 * @brief  Check if a Bluetooth device is currently connected.
 *         Detected by monitoring the STATE pin (if wired) or by RX activity.
 * @return true if connected.
 */
bool JDY31_IsConnected(void);

/**
 * @brief  Send a formatted data packet to the connected device.
 *         Format: "HW:<bpm>,<spo2>,<steps>,<hh:mm>\r\n"
 * @return JDY31_OK on success.
 */
JDY31_Status_t JDY31_SendPacket(const BlePacket_t *packet);

/**
 * @brief  Send a raw null-terminated string over UART.
 */
JDY31_Status_t JDY31_SendStr(const char *str);

/**
 * @brief  Send a raw byte buffer.
 */
JDY31_Status_t JDY31_SendRaw(const uint8_t *data, uint16_t len);

/**
 * @brief  Parse a complete line from the RX buffer into a BleCommand_t.
 *         Called by bleTask when a '\n' is received.
 * @param  line  Null-terminated received line (without \r\n).
 * @param  cmd   Output command struct.
 * @return BLE_CMD_NONE if no valid command found.
 */
BleCommandType_t JDY31_ParseCommand(const char *line, BleCommand_t *cmd);

/**
 * @brief  UART RX interrupt / DMA callback — call from HAL_UART_RxCpltCallback.
 *         Feeds the internal ring buffer.
 */
void JDY31_RxCallback(uint8_t byte);

/**
 * @brief  Attempt to read one complete line (\r\n terminated) from the RX buffer.
 * @param  buf      Output buffer.
 * @param  buf_size Size of output buffer.
 * @return Length of line read, 0 if no complete line available.
 */
uint16_t JDY31_ReadLine(char *buf, uint16_t buf_size);

/**
 * @brief  Flush both RX and TX buffers.
 */
void JDY31_FlushBuffers(void);

#ifdef __cplusplus
}
#endif

#endif /* __JDY31_H */
