/**
 * @file    jdy31.c
 * @brief   JDY-31 Bluetooth SPP module driver implementation.
 *
 * TODO (Phase 2 — Hardware Bring-up):
 *  - Test AT commands: send "AT\r\n" and expect "OK\r\n"
 *  - Verify "AT+NAME" changes the visible BT name on your phone
 *  - Enable UART RX interrupt: HAL_UART_Receive_IT() in JDY31_Init()
 *
 * TODO (Phase 4 — Integration):
 *  - Handle connection state via UART activity / STATE pin
 *  - Implement buffered TX for disconnect periods
 */

#include "jdy31.h"
#include "sensor_data.h"
#include "cmsis_os.h"
#include <string.h>
#include <stdio.h>

/* ========================================================================== *
 *  RX ring buffer
 * ========================================================================== */
static uint8_t  s_rx_buf[BLE_RX_BUF_SIZE];
static uint16_t s_rx_head = 0;
static uint16_t s_rx_tail = 0;
static uint8_t  s_rx_byte;   /* Single-byte DMA/IT receive target */

/* ========================================================================== *
 *  Connection state (approximate — no STATE pin assumed)
 * ========================================================================== */
static volatile bool s_connected = false;
static volatile uint32_t s_last_rx_tick = 0;

/* ========================================================================== *
 *  Init
 * ========================================================================== */
JDY31_Status_t JDY31_Init(void)
{
    JDY31_FlushBuffers();

    /* Start interrupt-driven single-byte RX */
    HAL_UART_Receive_IT(&BLE_UART_HANDLE, &s_rx_byte, 1);

    /* Optional: send AT commands to configure the module.
     * JDY-31 only enters AT mode when NOT connected to a host.
     * At power-on (no paired device) you can safely send: */
    osDelay(500); /* wait for module boot */

    /* TODO: Uncomment and test once hardware is verified
    JDY31_SendStr("AT+NAME" BLE_DEVICE_NAME "\r\n");
    osDelay(100);
    JDY31_SendStr("AT+BAUD4\r\n");   // 9600 baud
    osDelay(100);
    JDY31_SendStr("AT+RESET\r\n");
    osDelay(500);
    */

    return JDY31_OK;
}

/* ========================================================================== *
 *  Connection check (heuristic: recent RX activity)
 * ========================================================================== */
bool JDY31_IsConnected(void)
{
    /* TODO: If you wire the JDY-31 STATE pin to a GPIO, read it directly.
     * For now, assume connected if we received data in the last 10 seconds. */
    return s_connected;
}

/* ========================================================================== *
 *  Transmit
 * ========================================================================== */
JDY31_Status_t JDY31_SendPacket(const BlePacket_t *packet)
{
    char buf[BLE_TX_BUF_SIZE];
    int len = snprintf(buf, sizeof(buf),
                       "HW:%u,%u,%lu,%02u:%02u\r\n",
                       packet->bpm,
                       packet->spo2,
                       (unsigned long)packet->steps,
                       packet->hh,
                       packet->mm);
    if (len <= 0) return JDY31_ERR_UART;
    return JDY31_SendRaw((const uint8_t *)buf, (uint16_t)len);
}

JDY31_Status_t JDY31_SendStr(const char *str)
{
    return JDY31_SendRaw((const uint8_t *)str, (uint16_t)strlen(str));
}

JDY31_Status_t JDY31_SendRaw(const uint8_t *data, uint16_t len)
{
    HAL_StatusTypeDef ret = HAL_UART_Transmit(
        &BLE_UART_HANDLE, (uint8_t *)data, len, I2C_TIMEOUT_MS * 10);
    return (ret == HAL_OK) ? JDY31_OK : JDY31_ERR_UART;
}

/* ========================================================================== *
 *  RX callback (called from HAL_UART_RxCpltCallback in stm32f1xx_it.c)
 * ========================================================================== */
void JDY31_RxCallback(uint8_t byte)
{
    /* Store in ring buffer */
    uint16_t next = (s_rx_head + 1) % BLE_RX_BUF_SIZE;
    if (next != s_rx_tail) {   /* not full */
        s_rx_buf[s_rx_head] = byte;
        s_rx_head = next;
    }
    s_last_rx_tick = osKernelSysTick();
    s_connected = true; /* If we're receiving, we're connected */

    /* Re-arm interrupt for next byte */
    HAL_UART_Receive_IT(&BLE_UART_HANDLE, &s_rx_byte, 1);
}

/* ========================================================================== *
 *  Read a complete line from the RX ring buffer
 * ========================================================================== */
uint16_t JDY31_ReadLine(char *buf, uint16_t buf_size)
{
    uint16_t len = 0;
    uint16_t idx = s_rx_tail;

    /* Scan for '\n' in the ring buffer */
    while (idx != s_rx_head && len < buf_size - 1) {
        char c = (char)s_rx_buf[idx];
        idx = (idx + 1) % BLE_RX_BUF_SIZE;
        if (c == '\n') {
            buf[len] = '\0';
            s_rx_tail = idx; /* Consume up to and including '\n' */
            return len;
        }
        if (c != '\r') buf[len++] = c;
    }
    return 0; /* No complete line yet */
}

/* ========================================================================== *
 *  Command parser
 * ========================================================================== */
BleCommandType_t JDY31_ParseCommand(const char *line, BleCommand_t *cmd)
{
    cmd->type  = BLE_CMD_NONE;
    cmd->param = 0;

    if (strncmp(line, "SYNC_TIME:", 10) == 0) {
        cmd->type  = BLE_CMD_SYNC_TIME;
        cmd->param = (uint32_t)atoi(line + 10);
    } else if (strcmp(line, "RESET_STEPS") == 0) {
        cmd->type = BLE_CMD_RESET_STEPS;
    } else if (strcmp(line, "GET_DATA") == 0) {
        cmd->type = BLE_CMD_GET_DATA;
    } else if (strlen(line) > 0) {
        cmd->type = BLE_CMD_UNKNOWN;
    }

    return cmd->type;
}

void JDY31_FlushBuffers(void)
{
    s_rx_head = 0;
    s_rx_tail = 0;
    memset(s_rx_buf, 0, sizeof(s_rx_buf));
}
