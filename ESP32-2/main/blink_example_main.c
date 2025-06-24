// blink_example_main.c

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "sys/socket.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "netdb.h"
#include "arpa/inet.h"
#include "I2Sto.h"
#include <string.h>
#include "wifito.h"
#include "esp_log.h"
#include "webto.h"

// #include "wifito.h"

#include "esp_mn_models.h"
#include "model_path.h"
#include "esp_mn_iface.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_models.h"
#include "esp_websocket_client.h"
#include "text_storage.h"  // 添加头文件
#include "uart_send_to_stm32.h" // 添加头文件


// #include "freertos/ringbuf.h"

#define TAG "app"
// int16_t micBuffer[512];
// RingbufHandle_t buf_handle;
esp_websocket_client_handle_t handle;

bool detect_flag = false;
static esp_afe_sr_iface_t *afe_handle = NULL;
static volatile int task_flag = 0;

void feed_Task(void *arg)
{
    esp_afe_sr_data_t *afe_data = arg;

    int feed_chunksize = afe_handle->get_feed_chunksize(afe_data);
    int feed_nch = afe_handle->get_feed_channel_num(afe_data);
    int16_t *feed_buff = (int16_t *) malloc(feed_chunksize * feed_nch * sizeof(int16_t));
            

    assert(feed_buff);
    while (task_flag) {
        size_t bytesIn = 0;
        esp_err_t err = my_i2s_read(feed_buff, feed_chunksize * feed_nch * sizeof(int16_t), &bytesIn);

        afe_handle->feed(afe_data, feed_buff);
    }
    if (feed_buff) {
        free(feed_buff);
        feed_buff = NULL;
    }
    vTaskDelete(NULL);
}

void detect_Task(void *arg)
{

    esp_afe_sr_data_t *afe_data = arg;
    int afe_chunksize = afe_handle->get_fetch_chunksize(afe_data);
    int16_t *buff = malloc(afe_chunksize * sizeof(int16_t));
    assert(buff);
    printf("------------detect start------------\n");

    while (task_flag) {
        afe_fetch_result_t* res = afe_handle->fetch(afe_data); 

        if (!res || res->ret_value == ESP_FAIL) {
            printf("fetch error!\n");
            break;
        }

        if (res->wakeup_state == WAKENET_DETECTED) {
            printf("wakeword detected\n");
	        printf("model index:%d, word index:%d\n", res->wakenet_model_index, res->wake_word_index);
            printf("-----------LISTENING-----------\n");
            detect_flag = true;
        }
        if(detect_flag){
            websocket_sent_data(handle,res->data);
        }
    }
    if (buff) {
        free(buff);
        buff = NULL;
    }
    vTaskDelete(NULL);
}

int app_main()
{
    /* 初始化NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // 初始化外设
    my_i2s_init();
    wifi_init_sta();
    
    // 初始化UART
    uart_init_for_stm32();  // 确保UART已初始化

    
    handle = the_websocket_init();

    // 初始化模型
    srmodel_list_t *models = esp_srmodel_init("model");
    afe_config_t *afe_config = afe_config_init("M", models, AFE_TYPE_SR, AFE_MODE_LOW_COST);
    afe_handle = esp_afe_handle_from_config(afe_config);
    esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(afe_config);
    afe_config_free(afe_config);

    // 初始化文本存储
    text_storage_init();

    task_flag = 1;

    xTaskCreatePinnedToCore(&feed_Task, "feed", 8 * 1024, (void*)afe_data, 5, NULL, 0);
    xTaskCreatePinnedToCore(&detect_Task, "detect", 4 * 1024, (void*)afe_data, 5, NULL, 1);

    return 0;
}
