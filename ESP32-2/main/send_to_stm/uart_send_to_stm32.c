// uart_send_to_stm32.c

#include "uart_send_to_stm32.h" // 添加头文件
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "";

void uart_init_for_stm32(void) {
    // UART配置参数
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    // 配置UART参数
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    
    // 设置UART引脚
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, 
                               UART_TX_PIN, 
                               UART_RX_PIN, 
                               UART_PIN_NO_CHANGE, 
                               UART_PIN_NO_CHANGE));
    
    // 安装UART驱动程序
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, 
                                      UART_BUF_SIZE, 
                                      UART_BUF_SIZE, 
                                      UART_QUEUE_SIZE, 
                                      NULL, 
                                      0));
    
    ESP_LOGI(TAG, "UART2 initialized (TX: GPIO%d, RX: GPIO%d)", UART_TX_PIN, UART_RX_PIN);
}


void uart_send_data_to_stm32(const char *data, size_t len) {
    if (data == NULL || len == 0) {
        ESP_LOGE(TAG, "Invalid data pointer or zero length");
        return;
    }
    // 直接按指定长度发送
    int bytes_sent = uart_write_bytes(UART_PORT_NUM, data, len);
    
    if (bytes_sent < 0) {
        ESP_LOGE(TAG, "UART write error");
    } else {
        // 记录发送的字节数和内容摘要
        ESP_LOGI("UART_SEND", "Sent %d/%d bytes to STM32", bytes_sent, len);
        ESP_LOGI("UART_DEBUG", "Sending text: %s", data);
        
        // 记录前16个字节的十六进制值
        size_t log_len = (len > 16) ? 16 : len;
        ESP_LOG_BUFFER_HEX("UART_HEX", data, log_len);
        
        // 记录可打印字符
        char log_str[17] = {0};
        for (int i = 0; i < log_len; i++) {
            log_str[i] = (data[i] >= 32 && data[i] < 127) ? data[i] : '.';
        }
        ESP_LOGI("UART_STR", "[%.*s]", log_len, log_str);
    }
}

void send_to_stm32(char *data) 
{
    if (strlen(data) > 0) 
    {
        uart_write_bytes(UART_NUM_2, data, strlen(data));
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(data, "has been sent to stm32");
}