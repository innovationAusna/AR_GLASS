// text_storage.c

#include "text_storage.h"
#include "esp_log.h"
#include "uart_send_to_stm32.h"

#define MAX_TEXT_LENGTH 512
static char stored_text[MAX_TEXT_LENGTH] = {0};
static SemaphoreHandle_t text_mutex = NULL;
static const char *TAG = "TEXT_STORAGE";

void text_storage_init(void) {
    if (text_mutex == NULL) {
        text_mutex = xSemaphoreCreateMutex();
    }
    memset(stored_text, 0, MAX_TEXT_LENGTH);
}

void update_text(const char *new_text) {
    if (new_text == NULL) {
        return;
    }
    
    if (xSemaphoreTake(text_mutex, portMAX_DELAY) == pdTRUE) {
        // 复制新文本并添加换行符
        size_t len = strlen(new_text);
        if (len > MAX_TEXT_LENGTH - 2) {  // 保留空间给换行符和终止符
            len = MAX_TEXT_LENGTH - 2;
        }
        
        strncpy(stored_text, new_text, len);
        stored_text[len] = '\n';  // 添加换行符
        stored_text[len + 1] = '\0';  // 确保终止
        
        ESP_LOGI(TAG, "Updated text: %s", stored_text);
        
        xSemaphoreGive(text_mutex);
    }
}

void send_current_text(void) {
    if (xSemaphoreTake(text_mutex, portMAX_DELAY) == pdTRUE) {
        // 发送包含换行符的完整字符串
        size_t len = strlen(stored_text);
        
        if (len > 0) {
            uart_send_data_to_stm32(stored_text, len);
            ESP_LOGI(TAG, "Sent %d bytes to STM32 (including newline)", len);
        }
        
        xSemaphoreGive(text_mutex);
    }
}