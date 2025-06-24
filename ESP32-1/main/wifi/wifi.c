#include "receive.h"
#include "send.h"
#include "http.h"
#include "wifi.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/err.h"
#include "lwip/inet.h"
#include "lwip/udp.h"
#include "esp_netif.h"

#include "esp_http_server.h"
#include "cJSON.h"
#include "driver/gpio.h"

#define STORAGE_NAMESPACE "wifi_config"

// 按键 GPIO，假设 BOOT 按键连到 GPIO0，按下时拉低
#define CONFIG_BUTTON_GPIO GPIO_NUM_0

// AP 配置
#define AP_IP_ADDR "192.168.4.1"
#define AP_NETMASK "255.255.255.0"
#define AP_GATEWAY AP_IP_ADDR
#define AP_SSID_PREFIX "ESP_GYN"

// DNS 服务器端口
#define DNS_PORT 53

static const char *TAG = "wifi_config";

// 事件组位，用于 STA 连接
static EventGroupHandle_t s_wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;

// 全局保存 station 连接结果
static bool s_sta_connect_ok = false;

// 嵌入的 HTML
extern const unsigned char wifi_configuration_html_start[] asm("_binary_wifi_configuration_html_start");
extern const unsigned char wifi_configuration_html_end[]   asm("_binary_wifi_configuration_html_end");

// 简单 DNS 任务，将所有查询指向 AP IP
static void dns_server_task(void *arg) 
{
    struct sockaddr_in server_addr;
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "DNS socket create failed");
        vTaskDelete(NULL);
        return;
    }
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DNS_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) 
    {
        ESP_LOGE(TAG, "DNS socket bind failed");
        close(sock);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "DNS server started");
    while (1) {
        uint8_t buf[512];
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int recv_len = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&client_addr, &len);
        if (recv_len < 0) {
            continue;
        }
        // 设置回应标志和回答计数
        buf[2] |= 0x80;  // response flag
        buf[3] |= 0x80;  // recursion available
        buf[7] = 1;      // answer count = 1
        int offset = recv_len;
        // 指针到问题部分
        // 在 answer 部分写入指针 0xC00C, type A, class IN, TTL 60s, data length 4, IP
        // 0xC0 0x0C 表示指向报文头部偏移 12 字节处的域名
        buf[offset++] = 0xC0;
        buf[offset++] = 0x0C;
        buf[offset++] = 0x00;
        buf[offset++] = 0x01; // type A
        buf[offset++] = 0x00;
        buf[offset++] = 0x01; // class IN
        // TTL 60s
        buf[offset++] = 0x00;
        buf[offset++] = 0x00;
        buf[offset++] = 0x00;
        buf[offset++] = 0x3C;
        // data length = 4
        buf[offset++] = 0x00;
        buf[offset++] = 0x04;
        // IP 地址 192.168.4.1
        ip4_addr_t ip4;
        ip4.addr = inet_addr(AP_IP_ADDR);
        memcpy(&buf[offset], &ip4.addr, 4);
        offset += 4;
        sendto(sock, buf, offset, 0, (struct sockaddr *)&client_addr, len);
    }
    // never reach
    close(sock);
    vTaskDelete(NULL);
}

// HTTP 服务器 URI 处理

// GET "/" 返回配置页面
static esp_err_t index_get_handler(httpd_req_t *req) 
{
    httpd_resp_set_type(req, "text/html");
    // 防止浏览器缓存
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    const unsigned char *start = wifi_configuration_html_start;
    size_t len = wifi_configuration_html_end - wifi_configuration_html_start;
    httpd_resp_send(req, (const char *)start, len);
    return ESP_OK;
}


// POST "/submit", 接收 JSON {"ssid": "...", "password": "..."}
static esp_err_t submit_post_handler(httpd_req_t *req) 
{
    // 读取请求体
    int content_len = req->content_len;
    if (content_len <= 0 || content_len > 1024) 
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid length");
        return ESP_FAIL;
    }
    char *buf = malloc(content_len + 1);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No mem");
        return ESP_FAIL;
    }
    int received = httpd_req_recv(req, buf, content_len);
    if (received <= 0) 
    {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Recv err");
        return ESP_FAIL;
    }
    buf[received] = '\0';
    // 解析 JSON
    cJSON *json = cJSON_Parse(buf);
    free(buf);
    if (!json) 
    {
        httpd_resp_send(req, "{\"success\":false,\"error\":\"Invalid JSON\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    cJSON *ssid_item = cJSON_GetObjectItemCaseSensitive(json, "ssid");
    cJSON *pass_item = cJSON_GetObjectItemCaseSensitive(json, "password");
    if (!cJSON_IsString(ssid_item) || (ssid_item->valuestring == NULL)) 
    {
        cJSON_Delete(json);
        httpd_resp_send(req, "{\"success\":false,\"error\":\"Invalid SSID\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    const char *ssid = ssid_item->valuestring;
    const char *password = NULL;
    if (cJSON_IsString(pass_item) && pass_item->valuestring) 
    {
        password = pass_item->valuestring;
    }
    ESP_LOGI(TAG, "Received credentials: SSID=\"%s\"", ssid);
    // 尝试连接到该 SSID
    // 创建临时 WiFi STA 并尝试连接
    wifi_config_t wifi_cfg = { 0 };
    strncpy((char *)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid)-1);
    if (password) 
    {
        strncpy((char *)wifi_cfg.sta.password, password, sizeof(wifi_cfg.sta.password)-1);
    }
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_wifi_connect();
    // 等待连接结果，使用事件组
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));
    bool ok = false;
    if (bits & WIFI_CONNECTED_BIT) 
    {
        ESP_LOGI(TAG, "Connect OK");
        ok = true;
    } else 
    {
        ESP_LOGI(TAG, "Connect failed");
    }
    if (ok) 
    {
        // 保存到 NVS
        nvs_handle_t nvs;
        if (nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) 
        {
            nvs_set_str(nvs, "ssid", ssid);
            if (password) 
            {
                nvs_set_str(nvs, "password", password);
            } else 
            {
                nvs_erase_key(nvs, "password");
            }
            nvs_commit(nvs);
            nvs_close(nvs);
        }

        vTaskDelay(pdMS_TO_TICKS(10000));
        esp_restart();
    } else
    {
        // 返回失败
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"success\":false,\"error\":\"Connect failed\"}", HTTPD_RESP_USE_STRLEN);
    }
    cJSON_Delete(json);
    return ESP_OK;
}

// 重定向所有未匹配 URI 到 "/"
static esp_err_t default_get_handler(httpd_req_t *req) 
{
    // 302 重定向到根
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// 注册 HTTP 服务器及 URI
static httpd_handle_t start_http_server(void) 
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    config.uri_match_fn = httpd_uri_match_wildcard;
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) != ESP_OK) 
    {
        ESP_LOGE(TAG, "HTTP server start failed");
        return NULL;
    }
    // 注册 URI
    httpd_uri_t uri_index = 
    {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = index_get_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &uri_index);

    httpd_uri_t uri_submit = 
    {
        .uri       = "/submit",
        .method    = HTTP_POST,
        .handler   = submit_post_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &uri_submit);
    // 重定向其它常见 captive portal 请求
    const char* captive_list[] = 
    {
        "/hotspot-detect.html",
        "/generate_204",
        "/connectivity-check.html",
        "/ncsi.txt",
        "/fwlink/",
        "/success.txt",
        "/library/test/success.html"
    };
    for (int i = 0; i < sizeof(captive_list)/sizeof(captive_list[0]); i++) 
    {
        httpd_uri_t uri = 
        {
            .uri       = captive_list[i],
            .method    = HTTP_GET,
            .handler   = default_get_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &uri);
    }
    // 默认处理，重定向到 "/"
    httpd_uri_t uri_default = 
    {
        .uri       = "/*",
        .method    = HTTP_GET,
        .handler   = default_get_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &uri_default);
    return server;
}

// 事件处理，用于 STA 连接
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) 
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) 
    {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) 
    {
        ESP_LOGI(TAG, "STA disconnected");
        // 连接失败或断开，清除标志
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) 
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// 启动 STA 模式并尝试连接 NVS 中保存的 SSID/密码，返回是否成功
static bool start_wifi_sta_from_nvs(void) 
{
    nvs_handle_t nvs;
    char ssid[33] = {0};
    char password[65] = {0};
    bool has_ssid = false;
    if (nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &nvs) == ESP_OK) 
    {
        size_t ssid_len = sizeof(ssid);
        if (nvs_get_str(nvs, "ssid", ssid, &ssid_len) == ESP_OK) 
        {
            has_ssid = true;
        }
        size_t pass_len = sizeof(password);
        if (nvs_get_str(nvs, "password", password, &pass_len) != ESP_OK) 
        {
            password[0] = '\0';
        }
        nvs_close(nvs);
    }
    if (!has_ssid) 
    {
        ESP_LOGI(TAG, "No saved SSID");
        return false;
    }
    ESP_LOGI(TAG, "Trying to connect to saved SSID \"%s\"", ssid);
    // 初始化 WiFi STA
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip);
    esp_wifi_set_mode(WIFI_MODE_STA);
    wifi_config_t wifi_cfg = { 0 };
    strncpy((char *)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid)-1);
    if (password[0]) 
    {
        strncpy((char *)wifi_cfg.sta.password, password, sizeof(wifi_cfg.sta.password)-1);
    }
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    s_wifi_event_group = xEventGroupCreate();
    esp_wifi_start();
    // 等待连接结果，超时 10s
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));
    bool ok = false;
    if (bits & WIFI_CONNECTED_BIT) 
    {
        ESP_LOGI(TAG, "Connected to saved WiFi");
        ok = true;
    } else 
    {
        ESP_LOGI(TAG, "Failed to connect to saved WiFi");
        ok = false;
    }
    // 注销事件
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id);
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip);
    // 若连接失败，可停掉 WiFi
    if (!ok) 
    {
        esp_wifi_stop();
        esp_wifi_deinit();
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }
    return ok;
}

// 启动 AP 模式并运行配置服务，阻塞在此，直到设备重启
static void start_wifi_ap_config(void) 
{
    ESP_LOGI(TAG, "Starting AP config mode");
    // 初始化 TCP/IP
    esp_netif_init();
    // 创建默认 AP netif，并设置 IP
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    esp_netif_ip_info_t ip_info;
    inet_pton(AF_INET, AP_IP_ADDR, &ip_info.ip);
    inet_pton(AF_INET, AP_GATEWAY, &ip_info.gw);
    inet_pton(AF_INET, AP_NETMASK, &ip_info.netmask);
    esp_netif_dhcps_stop(ap_netif);
    esp_netif_set_ip_info(ap_netif, &ip_info);
    esp_netif_dhcps_start(ap_netif);
    // 初始化 WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    // 生成 SSID，例如 ESP_CFG-XXYY
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_AP, mac);
    char ap_ssid[32];
    snprintf(ap_ssid, sizeof(ap_ssid), "%s-%02X%02X", AP_SSID_PREFIX, mac[4], mac[5]);
    wifi_config_t ap_config = { 0 };
    strcpy((char *)ap_config.ap.ssid, ap_ssid);
    ap_config.ap.ssid_len = strlen(ap_ssid);
    ap_config.ap.max_connection = 4;
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_start();
    ESP_LOGI(TAG, "AP SSID:%s", ap_ssid);
    // 启动 DNS 劫持
    xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, NULL);
    // 注册 HTTP 服务器
    // 初始化事件循环，以便 HTTP 及 WiFi 事件等使用
    esp_event_loop_create_default();
    // HTTP 服务器
    httpd_handle_t server = start_http_server();
    if (!server) 
    {
        ESP_LOGE(TAG, "Failed to start HTTP server");
    }
    // 在此模式下，不主动退出，等待用户提交后在 submit 处理里 esp_restart()
    // 可以在此挂起任务
    while (1) 
    {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void wifi_init(void) 
{

    // 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // 配置 BOOT 按键（GPIO0）为输入，带上拉
    gpio_config_t io_conf = 
    {
        .pin_bit_mask = 1ULL << CONFIG_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // 初始化 TCP/IP 和默认 STA 接口
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    // 初始化 WiFi 驱动
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);

    // 注册事件处理
    s_wifi_event_group = xEventGroupCreate();
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

    // 从 NVS 中读取 Wi-Fi 配置
    nvs_handle_t nvs;
    char ssid[33] = {0};
    char password[65] = {0};
    bool has_ssid = false;

    if (nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &nvs) == ESP_OK) 
    {
        size_t len = sizeof(ssid);
        if (nvs_get_str(nvs, "ssid", ssid, &len) == ESP_OK) 
        {
            has_ssid = true;
        }
        len = sizeof(password);
        if (nvs_get_str(nvs, "password", password, &len) != ESP_OK) 
        {
            password[0] = '\0'; // 没有密码也可以连接开放网络
        }
        nvs_close(nvs);
    }

    if (!has_ssid) 
    {
        ESP_LOGI(TAG, "No saved Wi-Fi config found, entering config mode");
        start_wifi_ap_config();
        return;
    }

    // 设置 Wi-Fi STA 模式
    wifi_config_t wifi_cfg = { 0 };
    strncpy((char *)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char *)wifi_cfg.sta.password, password, sizeof(wifi_cfg.sta.password) - 1);
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_wifi_start();

    // 等待连接结果（最多10秒）
    ESP_LOGI(TAG, "Connecting to saved Wi-Fi: \"%s\"", ssid);
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));
    if (bits & WIFI_CONNECTED_BIT) 
    {
        ESP_LOGI(TAG, "Connected to Wi-Fi successfully");
        ESP_LOGI(TAG, "Device is now connected, ready for main application");
        send_to_stm32("function3\n");
        send_to_stm32("Connected to WiFi\n");
        send_to_stm32("start\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
        send_to_stm32("stop\n");
    } else 
    {
        ESP_LOGW(TAG, "Failed to connect to saved Wi-Fi, entering config mode");
        esp_wifi_stop();
        esp_wifi_deinit();
        vEventGroupDelete(s_wifi_event_group);
        start_wifi_ap_config();
    }
}