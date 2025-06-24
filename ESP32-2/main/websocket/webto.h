// webto.h

#ifndef _WEBTO_H_
#define _WEBTO_H_


#include <string.h>
#include "esp_event.h"

// 替换为你的WiFi信息
#define WIFI_SSID "Innovation"
#define WIFI_PASSWORD "fh051082699699"
#include "esp_websocket_client.h"

// WebSocket事件处理函数
static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
// 初始化WebSocket客户端
esp_websocket_client_handle_t the_websocket_init();

void websocket_sent_data(esp_websocket_client_handle_t ws_client, int16_t *data_byte_stream);


#endif