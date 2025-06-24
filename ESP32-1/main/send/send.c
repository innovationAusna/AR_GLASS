#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "send.h"
#include "string.h"

#define TXD_PIN (GPIO_NUM_11)
#define RXD_PIN (GPIO_NUM_10)

static const char *TAG = "UART";

void init_uart(void) 
{
    const uart_config_t uart_config = 
    {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    
    uart_driver_install(UART_NUM_2, 256, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_2, &uart_config);
    uart_set_pin(UART_NUM_2, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}


void send_to_stm32(char *data) 
{
    if (strlen(data) > 0) 
    {
        uart_write_bytes(UART_NUM_2, data, strlen(data));
    }
    vTaskDelay(pdMS_TO_TICKS(500));
}