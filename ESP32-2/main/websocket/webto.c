// webto.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_websocket_client.h"
#include "wifito.h"
#include "webto.h"
#include "text_storage.h"  // 添加头文件
#include "cJSON.h"  // 添加头文件
#include "http.h"
#include "uart_send_to_stm32.h"
#define BUFFER_SIZE 512

// 替换为你的WiFi信息
#define WIFI_SSID "Innovation"
#define WIFI_PASSWORD "fh051082699699"

// WebSocket服务器的URI
#define WEBSOCKET_URI "ws://172.20.10.11:27000/ws/transcribe?lang=auto"

// 日志标签
static const char *TAG = "websocket_client";

#define BUF_SIZE 512
char modified_question2[1024];
char modified_response2[1024];
typedef struct {
    char content[BUF_SIZE];
} ai_task_arg_t;

static void ai_chat_task(void *arg) {
    ai_task_arg_t *task_arg = (ai_task_arg_t *)arg;
    char response[BUF_SIZE];
    // 调用 HTTP 接口
    esp_err_t err = http_chat_with_ai(task_arg->content, response, sizeof(response));
    if (err == ESP_OK && strlen(response) > 0) {
        ESP_LOGI("AI_TASK", "Sending response to STM32: %s", response);
        // send_to_stm32("stop\n");
        send_to_stm32("function3\n");
        // send_to_stm32(response);
        snprintf(modified_response2, sizeof(modified_response2), "AI:%s", response);
        send_to_stm32(modified_response2);
        send_to_stm32("start\n");
        vTaskDelay(pdMS_TO_TICKS(10000));
        send_to_stm32("stop\n");
    } else {
        ESP_LOGW("AI_TASK", "AI response empty or error");
    }
    free(task_arg);
    vTaskDelete(NULL);
}

// WebSocket事件处理函数
static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) 
    {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WebSocket connected");
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "WebSocket disconnected");
            break;
        case WEBSOCKET_EVENT_DATA:
            ESP_LOGI(TAG, "Received data: %.*s", data->data_len, (char *)data->data_ptr);
            if (data->data_len > 1) 
            {
                cJSON *root = cJSON_Parse((char *)data->data_ptr);
                if (root) 
                {
                    // 提取 data 字段
                    cJSON *data_field = cJSON_GetObjectItem(root, "data");

                    if (data_field && cJSON_IsString(data_field)) 
                    {
                        const char *text = data_field->valuestring;
                        
                        // 更新文本并立即发送
                        // update_text(text);
                        // send_current_text();
                        
                        ESP_LOGI(TAG, "%s is Extracted text", text);

                        send_to_stm32("function3\n");
                        snprintf(modified_question2, sizeof(modified_question2), "User:%s\n", text);
                        send_to_stm32(modified_question2);
                        send_to_stm32("start\n");
                        // vTaskDelay(pdMS_TO_TICKS(500));
                        send_to_stm32("stop\n");
                        ai_task_arg_t *arg = malloc(sizeof(ai_task_arg_t));

                        if (arg) 
                        {
                            memset(arg, 0, sizeof(ai_task_arg_t));
                            size_t cpy = strlen(text);
                            if (cpy >= BUF_SIZE) cpy = BUF_SIZE - 1;
                            memcpy(arg->content, text, cpy);
                            arg->content[cpy] = '\0';
                            // 创建 FreeRTOS 任务
                            BaseType_t xReturned = xTaskCreate(ai_chat_task, "ai_chat_task", 4096, arg, 5, NULL);

                            if (xReturned != pdPASS) 
                            {
                                ESP_LOGE("Chatting with AI", "Failed to create AI chat task");
                                free(arg);
                            }
                        } 
                            
                        else 
                        {
                            ESP_LOGE("Chatting with AI", "Failed to malloc for task arg");
                        }
                        
                        

                        cJSON_Delete(root);
                    }
                }
            }
            break;
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WebSocket error");
            break;
        default:
            break;
    }
}


// 初始化WebSocket客户端
esp_websocket_client_handle_t the_websocket_init()
{
    const esp_websocket_client_config_t ws_cfg = {
        .uri = WEBSOCKET_URI,
    };
    esp_websocket_client_handle_t client = esp_websocket_client_init(&ws_cfg);
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, client);
    esp_websocket_client_start(client);
    return client;
}


void websocket_sent_data(esp_websocket_client_handle_t ws_client, int16_t *data_byte_stream)
{
    // 准备要发送的字节流数据



    int sent = esp_websocket_client_send_bin(ws_client, (const char *)data_byte_stream, BUFFER_SIZE, portMAX_DELAY);
    if (sent > 0) {
        ESP_LOGI(TAG, "Sent %d bytes of binary data", sent);
    } else {
        ESP_LOGE(TAG, "Failed to send binary data");
    }
}

