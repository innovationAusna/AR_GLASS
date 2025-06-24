// http.c

#include "string.h"
#include "sys/param.h"
#include "stdlib.h"
#include "ctype.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "protocol_examples_utils.h"
#include "esp_tls.h"
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "esp_http_client.h"
#include "cJSON.h" 

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048
static const char *TAG = "HTTP_CLIENT";
static char http_output_buffer[MAX_HTTP_OUTPUT_BUFFER];
static int http_output_len;
extern const char howsmyssl_com_root_cert_pem_start[] asm("_binary_howsmyssl_com_root_cert_pem_start");
extern const char howsmyssl_com_root_cert_pem_end[]   asm("_binary_howsmyssl_com_root_cert_pem_end");

extern const char postman_root_cert_pem_start[] asm("_binary_postman_root_cert_pem_start");
extern const char postman_root_cert_pem_end[]   asm("_binary_postman_root_cert_pem_end");

static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            printf("%.*s",evt->data_len,(char*)evt->data);
            if (!esp_http_client_is_chunked_response(evt->client)) 
            {
                int remain = MAX_HTTP_OUTPUT_BUFFER - http_output_len - 1;
                int copy_len = evt->data_len;
                if (copy_len > remain) 
                {
                    copy_len = remain;
                }
                if (copy_len > 0) 
                {
                    memcpy(http_output_buffer + http_output_len, evt->data, copy_len);
                    http_output_len += copy_len;
                    http_output_buffer[http_output_len] = '\0';
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}

// HTTP请求函数
esp_err_t http_rest_with_url(const char *user_input, char *out_buf, size_t out_buf_len)
{
    const char *api_url = "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";
    
    http_output_len = 0;
    http_output_buffer[0] = '\0';
    // 配置HTTPS客户端
    esp_http_client_config_t config = {
        .url = api_url,
        .event_handler = _http_event_handler,
        .disable_auto_redirect = false, // 启用自动重定向
        .timeout_ms = 10000, 
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach, // 证书验证
#endif
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if(!client)  
    {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return ESP_FAIL;
    }

    // 设置正确的请求头和认证
    // const char *post_data = "{\"model\":\"qwen-plus\",\"messages\":[{\"role\":\"user\",\"content\":\"你好，很高兴认识你\"}]}";
    char post_data[512];
    int len_snprintf = snprintf(post_data, sizeof(post_data),
             "{\"model\":\"qwen-plus\",\"messages\":[{\"role\":\"system\",\"content\":\"你是一个简练回答的AI助手，只用两三句英文回答用户问题。\"},{\"role\":\"user\",\"content\":\"%s\"}]}",
             user_input);
    if (len_snprintf < 0 || len_snprintf >= sizeof(post_data)) 
    {
        ESP_LOGE(TAG, "Post data too long or snprintf error");
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Authorization", "Bearer sk-18cd6f7f133648c7aae0940a43de931c"); 
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    // 执行请求
    esp_err_t err = esp_http_client_perform(client);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int status = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP status = %d, content length = %"PRIi64, status, esp_http_client_get_content_length(client));
    ESP_LOGD(TAG, "Full HTTP response: %s", http_output_buffer);

    if (http_output_len > 0) {
        cJSON *root = cJSON_Parse(http_output_buffer);
        if (root) {
            cJSON *choices = cJSON_GetObjectItem(root, "choices");
            if (choices && cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) 
            {
                cJSON *first = cJSON_GetArrayItem(choices, 0);
                if (first) 
                {
                    cJSON *message = cJSON_GetObjectItem(first, "message");
                    if (message) 
                    {
                        cJSON *content = cJSON_GetObjectItem(message, "content");
                        if (content && cJSON_IsString(content) && content->valuestring) 
                        {
                            size_t src_len = strlen(content->valuestring);
                            if (out_buf_len >= 2) 
                            {
                                size_t max_copy = out_buf_len - 2; // 为 '\n' 和 '\0' 留空间
                                size_t copy_len = (src_len < max_copy) ? src_len : max_copy;
                                memcpy(out_buf, content->valuestring, copy_len);
                                out_buf[copy_len] = '\n';
                                out_buf[copy_len + 1] = '\0';
                            } else 
                            {
                                // out_buf_len 太小，无法同时放内容、换行和终止符
                                if (out_buf_len >= 1) 
                                {
                                    out_buf[0] = '\0';
                                }
                            }
                        }
                    }
                }
            } else 
            {
                ESP_LOGW(TAG, "No choices or empty array in JSON");
                if (out_buf_len>0) out_buf[0] = '\0';
            }
            cJSON_Delete(root);
        } else {
            ESP_LOGE(TAG, "Failed to parse JSON");
            if (out_buf_len>0) out_buf[0] = '\0';
        }
    } else {
        ESP_LOGW(TAG, "Empty HTTP response");
        if (out_buf_len>0) out_buf[0] = '\0';
    }

    esp_http_client_cleanup(client); // 正确清理资源
    return ESP_OK;
}

esp_err_t http_chat_with_ai(const char *content, char *out_buf, size_t out_buf_len)
{
    ESP_LOGI(TAG, "Start http_chat_with_ai");
    esp_err_t err = http_rest_with_url(content, out_buf, out_buf_len);
    if (err == ESP_OK) 
    {
        ESP_LOGI(TAG, "AI response: %s", out_buf);
    } else 
    {
        ESP_LOGE(TAG, "http_chat_with_ai failed: %s", esp_err_to_name(err));
        if (out_buf_len > 0) out_buf[0] = '\0';
    }
    return err;
}