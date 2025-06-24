// uart_send_to_stm32.h

#ifndef UART_SEND_TO_STM32_H
#define UART_SEND_TO_STM32_H

#include "driver/uart.h"

// UART配置
#define UART_PORT_NUM         UART_NUM_2
#define UART_TX_PIN           11
#define UART_RX_PIN           10
#define UART_BAUD_RATE        115200
#define UART_BUF_SIZE         1024
#define UART_QUEUE_SIZE       10

/**
 * @brief 初始化UART2用于与STM32通信
 */
void uart_init_for_stm32(void);

/**
 * @brief 通过UART向STM32发送字符串数据
 * 
 * @param data 要发送的字符串数据
 */
void uart_send_data_to_stm32(const char *data, size_t len);
void send_to_stm32(char *data);

#endif // UART_SEND_TO_STM32_H