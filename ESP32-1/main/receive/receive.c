#include "receive.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "string.h"
#include "send.h"
#include "http.h"

#define GATTS_TABLE_TAG "AR_GLASS"
#define GATTS_DEMO_CHAR_VAL_LEN_MAX 500  // 最大单次写入长度
#define PREPARE_BUF_MAX_SIZE        1024 // 分片写入缓冲区大小
#define CHAR_DECLARATION_SIZE       sizeof(uint8_t)

#define ADV_CONFIG_FLAG             (1 << 0)
#define SCAN_RSP_CONFIG_FLAG        (1 << 1)

#define BUF_SIZE 512
static char teleprompter_str[BUF_SIZE] = {0};
static char distance_str[BUF_SIZE] = {0};
static char direction_str[BUF_SIZE] = {0};
static char ai_str[BUF_SIZE] = {0};
char function_str[BUF_SIZE] = {0};
char modified_question[1024];
char modified_response[1024];

static uint8_t adv_config_done = 0;

// 接收数据缓冲区
static uint8_t received_data[GATTS_DEMO_CHAR_VAL_LEN_MAX];

// 广播数据
static uint8_t raw_adv_data[] = 
{
    0x02, 0x01, 0x06,        // Flags: LE General Discoverable
    0x02, 0x0a, 0xeb,        // TX Power
    0x09, 0x03,              // 服务UUID列表（长度11字节）
    0xFF, 0x00,              // Service1: 0x00FF
    0xAA, 0x00,              // Service2: 0x00AA
    0xBB, 0x00,              // Service3: 0x00BB
    0xCC, 0x00,              // Service4: 0x00CC 
    0x09, 0x09, 'A', 'R', '_', 'G', 'L', 'A', 'S', 'S' // Device name
};
static uint8_t raw_scan_rsp_data[] = 
{
    0x02, 0x01, 0x06,        // Flags
    0x02, 0x0a, 0xeb,        // TX Power
    0x09, 0x03,              // 服务UUID列表（长度11字节）
    0xFF, 0x00,              // Service1: 0x00FF
    0xAA, 0x00,              // Service2: 0x00AA
    0xBB, 0x00,              // Service3: 0x00BB
    0xCC, 0x00,              // Service4: 0x00CC 
};
static esp_ble_adv_params_t adv_params = 
{
    .adv_int_min         = 0x20,
    .adv_int_max         = 0x40,
    .adv_type            = ADV_TYPE_IND,
    .own_addr_type       = BLE_ADDR_TYPE_PUBLIC,
    .channel_map         = ADV_CHNL_ALL,
    .adv_filter_policy   = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// --- UUID 定义 ---
// Service1
static const uint16_t GATTS_SERVICE_UUID_TEST      = 0x00FF;
static const uint16_t GATTS_CHAR_UUID_TEST_C       = 0xFF03;
// Service2
static const uint16_t GATTS_SERVICE2_UUID          = 0x00AA;
static const uint16_t GATTS_CHAR_UUID_SVC2_C1      = 0xAA01;
static const uint16_t GATTS_CHAR_UUID_SVC2_C2      = 0xAA02;

// Service3
static const uint16_t GATTS_SERVICE3_UUID          = 0x00BB;
static const uint16_t GATTS_CHAR_UUID_SVC3_C1      = 0xBB01;

// Service4
static const uint16_t GATTS_SERVICE4_UUID          = 0x00CC; 
static const uint16_t GATTS_CHAR_UUID_SVC4_C1      = 0xCC01; 

static const uint16_t primary_service_uuid         = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid   = ESP_GATT_UUID_CHAR_DECLARE;
static const uint8_t char_prop_write               = ESP_GATT_CHAR_PROP_BIT_WRITE;
static const uint8_t char_value_init[1]            = {0x00}; 

// --- 为 Service1 定义 GATT DB，单个 Service，只含一个写特性 --- 
enum 
{
    IDX_SVC1_SVC,        // 服务声明
    IDX_SVC1_CHAR_DECL,  // 特性声明
    IDX_SVC1_CHAR_VAL,   // 特性值
    SVC1_IDX_NB
};


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
        send_to_stm32("stop\n");
        send_to_stm32("functio3\n");
        // send_to_stm32(response);
        snprintf(modified_response, sizeof(modified_response), "AI:%s", response);
        send_to_stm32(modified_response);
        send_to_stm32("start\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
        send_to_stm32("stop\n");
    } else {
        ESP_LOGW("AI_TASK", "AI response empty or error");
    }
    free(task_arg);
    vTaskDelete(NULL);
}

static const esp_gatts_attr_db_t gatt_db_svc1[SVC1_IDX_NB] = 
{
    // Service1 Declaration
    [IDX_SVC1_SVC] =
    {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
         sizeof(uint16_t), sizeof(GATTS_SERVICE_UUID_TEST), (uint8_t *)&GATTS_SERVICE_UUID_TEST}
    },
    // Service1 特性声明 (Write)
    [IDX_SVC1_CHAR_DECL] = 
    {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
         CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_write}
    },
    // Service1 特性值 (Write)
    [IDX_SVC1_CHAR_VAL] = 
    {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_TEST_C, ESP_GATT_PERM_WRITE,
         GATTS_DEMO_CHAR_VAL_LEN_MAX, sizeof(char_value_init), (uint8_t *)char_value_init}
    },
};

// --- 为 Service2 定义 GATT DB，含两个写特性 ---
enum 
{
    IDX_SVC2_SVC,         // 服务声明
    IDX_SVC2_CHAR1_DECL,  // 特性1声明
    IDX_SVC2_CHAR1_VAL,   // 特性1值
    IDX_SVC2_CHAR2_DECL,  // 特性2声明
    IDX_SVC2_CHAR2_VAL,   // 特性2值
    SVC2_IDX_NB
};
static const esp_gatts_attr_db_t gatt_db_svc2[SVC2_IDX_NB] = 
{
    // Service2 Declaration
    [IDX_SVC2_SVC] = 
    {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
         sizeof(uint16_t), sizeof(GATTS_SERVICE2_UUID), (uint8_t *)&GATTS_SERVICE2_UUID}
    },
    // Service2 特性1声明 (Write)
    [IDX_SVC2_CHAR1_DECL] = 
    {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
         CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_write}
    },
    // Service2 特性1值 (Write)
    [IDX_SVC2_CHAR1_VAL] = 
    {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_SVC2_C1, ESP_GATT_PERM_WRITE,
         GATTS_DEMO_CHAR_VAL_LEN_MAX, 0, NULL}
    },
    // Service2 特性2声明 (Write)
    [IDX_SVC2_CHAR2_DECL] = 
    {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
         CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_write}
    },
    // Service2 特性2值 (Write)
    [IDX_SVC2_CHAR2_VAL] = 
    {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_SVC2_C2, ESP_GATT_PERM_WRITE,
         GATTS_DEMO_CHAR_VAL_LEN_MAX, 0, NULL}
    },
};

// --- 为 Service3 定义 GATT DB，单个可写特性 ---
enum 
{
    IDX_SVC3_SVC,        // 服务声明
    IDX_SVC3_CHAR_DECL,  // 特性声明
    IDX_SVC3_CHAR_VAL,   // 特性值
    SVC3_IDX_NB
};
static const esp_gatts_attr_db_t gatt_db_svc3[SVC3_IDX_NB] = 
{
    // Service3 Declaration
    [IDX_SVC3_SVC] = 
    {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
         sizeof(uint16_t), sizeof(GATTS_SERVICE3_UUID), (uint8_t *)&GATTS_SERVICE3_UUID}
    },
    // Service3 特性声明 (Write)
    [IDX_SVC3_CHAR_DECL] = 
    {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
         CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_write}
    },
    // Service3 特性值 (Write)
    [IDX_SVC3_CHAR_VAL] = 
    {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_SVC3_C1, ESP_GATT_PERM_WRITE,
         GATTS_DEMO_CHAR_VAL_LEN_MAX, 0, NULL}  
    },
};

// --- 为 Service4 定义 GATT DB，单个可写特性 ---
enum 
{
    IDX_SVC4_SVC,        // 服务声明
    IDX_SVC4_CHAR_DECL,  // 特性声明
    IDX_SVC4_CHAR_VAL,   // 特性值
    SVC4_IDX_NB
};

static const esp_gatts_attr_db_t gatt_db_svc4[SVC4_IDX_NB] = 
{
    // Service4 Declaration
    [IDX_SVC4_SVC] = 
    {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
         sizeof(uint16_t), sizeof(GATTS_SERVICE4_UUID), (uint8_t *)&GATTS_SERVICE4_UUID}
    },
    // Service4 特性声明 (Write)
    [IDX_SVC4_CHAR_DECL] = 
    {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
         CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_write}
    },
    // Service4 特性值 (Write)
    [IDX_SVC4_CHAR_VAL] = 
    {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_SVC4_C1, ESP_GATT_PERM_WRITE,
         GATTS_DEMO_CHAR_VAL_LEN_MAX, 0, NULL}  
    },
};

// --- Handle 表 ---
static uint16_t svc1_handle_table[SVC1_IDX_NB];
static uint16_t svc2_handle_table[SVC2_IDX_NB];
static uint16_t svc3_handle_table[SVC3_IDX_NB];
static uint16_t svc4_handle_table[SVC4_IDX_NB]; 

// --- Prepare Write Env: 为每个可分片写特性单独准备 ---
typedef struct 
{
    uint8_t *prepare_buf;
    int prepare_len;
} prepare_type_env_t;
static prepare_type_env_t prepare_env_svc1;
static prepare_type_env_t prepare_env_svc2_c1;
static prepare_type_env_t prepare_env_svc2_c2;
static prepare_type_env_t prepare_env_svc3_c1;
static prepare_type_env_t prepare_env_svc4_c1; 

// GAP 事件处理
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) 
{
    switch (event) 
    {
        case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
            adv_config_done &= ~ADV_CONFIG_FLAG;
            if (adv_config_done == 0) 
            {
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
            adv_config_done &= ~SCAN_RSP_CONFIG_FLAG;
            if (adv_config_done == 0) 
            {
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) 
            {
                ESP_LOGE(GATTS_TABLE_TAG, "Advertising start failed");
            } else 
            {
                ESP_LOGI(GATTS_TABLE_TAG, "Advertising started");
            }
            break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "Connection params updated");
            break;
        default:
            break;
    }
}

// Prepare Write 通用处理，单独 buffer
static void example_prepare_write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *env, 
                                           esp_ble_gatts_cb_param_t *param) {
    if (!env->prepare_buf) 
    {
        env->prepare_buf = (uint8_t *)malloc(PREPARE_BUF_MAX_SIZE);
        env->prepare_len = 0;
        if (!env->prepare_buf) 
        {
            ESP_LOGE(GATTS_TABLE_TAG, "Prepare buffer allocation failed");
            return;
        }
    }
    if (param->write.offset > PREPARE_BUF_MAX_SIZE ||
        (param->write.offset + param->write.len) > PREPARE_BUF_MAX_SIZE) 
        {
        ESP_LOGE(GATTS_TABLE_TAG, "Invalid write offset/length");
        return;
    }
    memcpy(env->prepare_buf + param->write.offset, param->write.value, param->write.len);
    env->prepare_len += param->write.len;
    if (param->write.need_rsp) 
    {
        esp_gatt_rsp_t rsp = 
        {
            .attr_value = 
            {
                .len = param->write.len,
                .handle = param->write.handle,
                .offset = param->write.offset,
                .auth_req = ESP_GATT_AUTH_REQ_NONE
            }
        };
        memcpy(rsp.attr_value.value, param->write.value, param->write.len);
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
                                   param->write.trans_id, ESP_GATT_OK, &rsp);
    }
}

// Execute Write 通用处理：将完整数据复制到 received_data 并处理，然后 free buffer
static void example_exec_write_event_env(prepare_type_env_t *env, uint16_t char_handle) 
{
    if (env->prepare_buf) 
    {
        // 复制到 received_data
        size_t valid_len = (env->prepare_len > sizeof(received_data)) ? sizeof(received_data) : env->prepare_len;
        memcpy(received_data, env->prepare_buf, valid_len);
        // 日志输出
        ESP_LOGI(GATTS_TABLE_TAG, "Long write complete on handle %d, %d bytes", char_handle, (int)valid_len);
        ESP_LOG_BUFFER_HEX(GATTS_TABLE_TAG, received_data, valid_len);

        // 当作字符串处理并 send
        size_t copy_len = (valid_len > (BUF_SIZE - 2)) ? (BUF_SIZE - 2) : valid_len;
        memcpy(teleprompter_str, received_data, copy_len);
        teleprompter_str[copy_len] = '\n';
        teleprompter_str[copy_len+1] = '\0';
        ESP_LOGI("TELEPROMPTER", "%s", teleprompter_str);
        send_to_stm32("function1\n");
        send_to_stm32(teleprompter_str);
        // 清理
        free(env->prepare_buf);
        env->prepare_buf = NULL;
        env->prepare_len = 0;
    }
}

// 标记 Service 是否已创建
static bool service1_created = false;
static bool service2_created = false;
static bool service3_created = false;
static bool service4_created = false;

// GATTS 事件处理
static void gatts_event_handler(esp_gatts_cb_event_t event,
                                esp_gatt_if_t gatts_if,
                                esp_ble_gatts_cb_param_t *param) 
{
    switch (event) 
    {
        case ESP_GATTS_REG_EVT:
            if (param->reg.status == ESP_GATT_OK) 
            {
                // 设置设备名称，只需在第一个 Service 创建时做一次
                esp_ble_gap_set_device_name("AR_GLASS");
                // 配置广播数据、扫描响应
                esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
                adv_config_done |= ADV_CONFIG_FLAG;
                esp_ble_gap_config_scan_rsp_data_raw(raw_scan_rsp_data, sizeof(raw_scan_rsp_data));
                adv_config_done |= SCAN_RSP_CONFIG_FLAG;
                // 首先创建 Service1 的属性表
                esp_ble_gatts_create_attr_tab(gatt_db_svc1, gatts_if, SVC1_IDX_NB, 0);
            }
            break;

        case ESP_GATTS_CREAT_ATTR_TAB_EVT:
            if (param->add_attr_tab.status != ESP_GATT_OK) 
            {
                ESP_LOGE(GATTS_TABLE_TAG, "CREATE_ATTR_TAB failed, status %d", param->add_attr_tab.status);
                break;
            }
            if (!service1_created) 
            {
                // 这是 Service1 的属性表结果
                service1_created = true;
                // 保存 Service1 handles
                memcpy(svc1_handle_table, param->add_attr_tab.handles, sizeof(svc1_handle_table));
                ESP_LOGI(GATTS_TABLE_TAG, "Service1 created, handle=%d", svc1_handle_table[IDX_SVC1_SVC]);
                // 启动 Service1
                esp_ble_gatts_start_service(svc1_handle_table[IDX_SVC1_SVC]);
                // 打印特性 handle（可选）
                ESP_LOGI(GATTS_TABLE_TAG, "Service1 Char handle=%d", svc1_handle_table[IDX_SVC1_CHAR_VAL]);
                // 创建 Service2的属性表
                esp_ble_gatts_create_attr_tab(gatt_db_svc2, gatts_if, SVC2_IDX_NB, 0);
                
            } else if (!service2_created)
            {
                // 这是 Service2 的属性表结果
                service2_created = true;
                memcpy(svc2_handle_table, param->add_attr_tab.handles, sizeof(svc2_handle_table));
                ESP_LOGI(GATTS_TABLE_TAG, "Service2 created, handle=%d", svc2_handle_table[IDX_SVC2_SVC]);
                // 启动 Service2
                esp_ble_gatts_start_service(svc2_handle_table[IDX_SVC2_SVC]);
                ESP_LOGI(GATTS_TABLE_TAG, "Service2 Char1 handle=%d", svc2_handle_table[IDX_SVC2_CHAR1_VAL]);
                ESP_LOGI(GATTS_TABLE_TAG, "Service2 Char2 handle=%d", svc2_handle_table[IDX_SVC2_CHAR2_VAL]);
                // 创建 Service3的属性表
                esp_ble_gatts_create_attr_tab(gatt_db_svc3, gatts_if, SVC3_IDX_NB, 0);
            } else if (!service3_created)
            {
                service3_created = true;
                memcpy(svc3_handle_table, param->add_attr_tab.handles, sizeof(svc3_handle_table));
                ESP_LOGI(GATTS_TABLE_TAG, "Service3 created, handle=%d", svc3_handle_table[IDX_SVC3_SVC]);
                esp_ble_gatts_start_service(svc3_handle_table[IDX_SVC3_SVC]);
                ESP_LOGI(GATTS_TABLE_TAG, "Service3 Char handle=%d", svc3_handle_table[IDX_SVC3_CHAR_VAL]);
                // 创建 Service4的属性表
                esp_ble_gatts_create_attr_tab(gatt_db_svc4, gatts_if, SVC4_IDX_NB, 0);
            } else if (!service4_created) 
            {
                service4_created = true;
                memcpy(svc4_handle_table, param->add_attr_tab.handles, sizeof(svc4_handle_table));
                ESP_LOGI(GATTS_TABLE_TAG, "Service4 created, handle=%d", svc4_handle_table[IDX_SVC4_SVC]);
                esp_ble_gatts_start_service(svc4_handle_table[IDX_SVC4_SVC]);
                ESP_LOGI(GATTS_TABLE_TAG, "Service4 Char handle=%d", svc4_handle_table[IDX_SVC4_CHAR_VAL]);
            }
            break;

        case ESP_GATTS_READ_EVT:
            // 如有读特性，可在此处理。当前全为写特性，故只返回 OK
            esp_ble_gatts_send_response(gatts_if, param->read.conn_id,
                                       param->read.trans_id, ESP_GATT_OK, NULL);
            break;

        case ESP_GATTS_WRITE_EVT: 
        {
            uint16_t handle = param->write.handle;
            if (!param->write.is_prep) 
            {
                // 普通写
                // Service1 特性1 写
                if (handle == svc1_handle_table[IDX_SVC1_CHAR_VAL]) 
                {
                    size_t len = param->write.len;
                    size_t valid_len = (len > sizeof(received_data)) ? sizeof(received_data) : len;
                    memcpy(received_data, param->write.value, valid_len);
                    ESP_LOGI("TELEPROMPTER", "Service1 Char Received %d bytes", valid_len);
                    ESP_LOG_BUFFER_HEX("TELEPROMPTER", received_data, valid_len);
                    // 当作字符串处理并 send
                    size_t copy_len = (valid_len > (BUF_SIZE - 2)) ? (BUF_SIZE - 2) : valid_len;
                    memcpy(teleprompter_str, received_data, copy_len);
                    teleprompter_str[copy_len] = '\n';
                    teleprompter_str[copy_len+1] = '\0';
                    ESP_LOGI("TELEPROMPTER", "%s", teleprompter_str);
                    send_to_stm32("function1\n");
                    send_to_stm32(teleprompter_str);
                }
                // Service2 特性1 写
                else if (handle == svc2_handle_table[IDX_SVC2_CHAR1_VAL]) 
                {
                    size_t len = param->write.len;
                    size_t valid_len = (len > sizeof(received_data)) ? sizeof(received_data) : len;
                    memcpy(received_data, param->write.value, valid_len);
                    ESP_LOGI("NAVIGATOR_DISTANCE", "Service2 Char1 Received %d bytes", valid_len);
                    ESP_LOG_BUFFER_HEX("NAVIGATOR_DISTANCE", received_data, valid_len);
                    // 当作字符串处理并 send
                    size_t copy_len = (valid_len > (BUF_SIZE - 2)) ? (BUF_SIZE - 2) : valid_len;
                    memcpy(distance_str, received_data, copy_len);
                    distance_str[copy_len] = '\n';
                    distance_str[copy_len+1] = '\0';
                    ESP_LOGI("NAVIGATOR_DISTANCE", "%s", distance_str);
                    send_to_stm32("function2\n");
                    send_to_stm32(distance_str);
                }
                // Service2 特性2 写
                else if (handle == svc2_handle_table[IDX_SVC2_CHAR2_VAL]) 
                {
                    size_t len = param->write.len;
                    size_t valid_len = (len > sizeof(received_data)) ? sizeof(received_data) : len;
                    memcpy(received_data, param->write.value, valid_len);
                    ESP_LOGI("NAVIGATOR_DIRECTION", "Service2 Char2 Received %d bytes", valid_len);
                    ESP_LOG_BUFFER_HEX("NAVIGATOR_DIRECTION", received_data, valid_len);
                    // 当作字符串处理并 send
                    size_t copy_len = (valid_len > (BUF_SIZE - 2)) ? (BUF_SIZE - 2) : valid_len;
                    memcpy(direction_str, received_data, copy_len);
                    direction_str[copy_len] = '\n';
                    direction_str[copy_len+1] = '\0';
                    ESP_LOGI("NAVIGATOR_DIRECTION", "%s", direction_str);
                    send_to_stm32("function2\n");
                    send_to_stm32(direction_str);
                }
                // Service3 特性1 写
                else if (handle == svc3_handle_table[IDX_SVC3_CHAR_VAL]) 
                {
                    size_t len = param->write.len;
                    size_t valid_len = (len > sizeof(received_data)) ? sizeof(received_data) : len;
                    memcpy(received_data, param->write.value, valid_len);
                    ESP_LOGI("Chatting with AI", "Service3 Char Received %d bytes", valid_len);
                    ESP_LOG_BUFFER_HEX("Chatting with AI", received_data, valid_len);
                    // 当作字符串处理并 send
                    size_t copy_len = (valid_len > (BUF_SIZE - 2)) ? (BUF_SIZE - 2) : valid_len;
                    memcpy(ai_str, received_data, copy_len);
                    ai_str[copy_len] = '\n';
                    ai_str[copy_len+1] = '\0';
                    ESP_LOGI("Chatting with AI", "User says: %s", ai_str);
                    send_to_stm32("function3\n");
                    snprintf(modified_question, sizeof(modified_question), "User:%s", ai_str);
                    send_to_stm32(modified_question);
                    // send_to_stm32(ai_str);
                    send_to_stm32("start\n");
                    // http_chat_with_ai(ai_str);
                    // vTaskDelay(pdMS_TO_TICKS(5000));
                    // send_to_stm32("function1\n");
                    // ESP_LOGI("AI responds:", "%s", ai_response);
                    // send_to_stm32(ai_response);
                    // 创建任务参数
                    ai_task_arg_t *arg = malloc(sizeof(ai_task_arg_t));
                    if (arg) {
                        memset(arg, 0, sizeof(ai_task_arg_t));
                        size_t cpy = copy_len;
                        if (cpy >= BUF_SIZE) cpy = BUF_SIZE - 1;
                        memcpy(arg->content, ai_str, cpy);
                        arg->content[cpy] = '\0';
                        // 创建 FreeRTOS 任务
                        BaseType_t xReturned = xTaskCreate(ai_chat_task, "ai_chat_task", 4096, arg, 5, NULL);
                        if (xReturned != pdPASS) 
                        {
                            ESP_LOGE("Chatting with AI", "Failed to create AI chat task");
                            free(arg);
                        }
                    } else 
                    {
                        ESP_LOGE("Chatting with AI", "Failed to malloc for task arg");
                    }

                    if (param->write.need_rsp) 
                    {
                        esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
                                                param->write.trans_id, ESP_GATT_OK, NULL);
                    }
                } else if (handle == svc4_handle_table[IDX_SVC4_CHAR_VAL]) 
                {
                    size_t len = param->write.len;
                    size_t valid_len = (len > sizeof(received_data)) ? sizeof(received_data) : len;
                    memcpy(received_data, param->write.value, valid_len);
                    ESP_LOGI("FUNCTION", "Service4 Char1 Received %d bytes", valid_len);
                    ESP_LOG_BUFFER_HEX("FUNCTION", received_data, valid_len);
                    // 当作字符串处理并 send
                    size_t copy_len = (valid_len > (BUF_SIZE - 2)) ? (BUF_SIZE - 2) : valid_len;
                    memcpy(function_str, received_data, copy_len);
                    function_str[copy_len] = '\n';
                    function_str[copy_len+1] = '\0';
                    ESP_LOGI("FUNCTION", "%s", function_str);
                    send_to_stm32(function_str);
                }
                // 其他 handle 可忽略或扩展
                if (param->write.need_rsp) 
                {
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
                                               param->write.trans_id, ESP_GATT_OK, NULL);
                }
            } else 
            {
                // 分片写 (Prepare Write)，根据 handle 分发到不同 env
                if (handle == svc1_handle_table[IDX_SVC1_CHAR_VAL]) 
                {
                    example_prepare_write_event_env(gatts_if, &prepare_env_svc1, param);
                } else if (handle == svc2_handle_table[IDX_SVC2_CHAR1_VAL]) 
                {
                    example_prepare_write_event_env(gatts_if, &prepare_env_svc2_c1, param);
                } else if (handle == svc2_handle_table[IDX_SVC2_CHAR2_VAL]) 
                {
                    example_prepare_write_event_env(gatts_if, &prepare_env_svc2_c2, param);
                } else if (handle == svc3_handle_table[IDX_SVC3_CHAR_VAL])
                {
                    example_prepare_write_event_env(gatts_if, &prepare_env_svc3_c1, param);
                } else if (handle == svc4_handle_table[IDX_SVC4_CHAR_VAL]) 
                {
                    example_prepare_write_event_env(gatts_if, &prepare_env_svc4_c1, param);
                }
            }
            break;
        }
        case ESP_GATTS_EXEC_WRITE_EVT:
            // Execute Write: 对各个 env 分别处理
            if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC) 
            {
                // Service1 长写
                example_exec_write_event_env(&prepare_env_svc1, svc1_handle_table[IDX_SVC1_CHAR_VAL]);
                // Service2 特性1 长写
                example_exec_write_event_env(&prepare_env_svc2_c1, svc2_handle_table[IDX_SVC2_CHAR1_VAL]);
                // Service2 特性2 长写
                example_exec_write_event_env(&prepare_env_svc2_c2, svc2_handle_table[IDX_SVC2_CHAR2_VAL]);
                // Service3 特性1 长写
                example_exec_write_event_env(&prepare_env_svc3_c1, svc3_handle_table[IDX_SVC3_CHAR_VAL]);
                // Service4 特性1 长写
                example_exec_write_event_env(&prepare_env_svc4_c1, svc4_handle_table[IDX_SVC4_CHAR_VAL]);
            }
            break;

        case ESP_GATTS_MTU_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "MTU size: %d", param->mtu.mtu);
            break;
        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "Device connected");
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "Device disconnected");
            esp_ble_gap_start_advertising(&adv_params);
            break;
        default:
            break;
    }
}

void receive_init(void) 
{
    // 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 释放经典蓝牙内存
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    // 初始化蓝牙控制器
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) 
    {
        ESP_LOGE(GATTS_TABLE_TAG, "Controller init failed: %s", esp_err_to_name(ret));
        return;
    }
    // 启用 BLE 模式
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) 
    {
        ESP_LOGE(GATTS_TABLE_TAG, "Controller enable failed: %s", esp_err_to_name(ret));
        return;
    }
    // 初始化蓝牙协议栈
    ret = esp_bluedroid_init();
    if (ret) 
    {
        ESP_LOGE(GATTS_TABLE_TAG, "Bluedroid init failed: %s", esp_err_to_name(ret));
        return;
    }
    // 启用蓝牙协议栈
    ret = esp_bluedroid_enable();
    if (ret) 
    {
        ESP_LOGE(GATTS_TABLE_TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret));
        return;
    }
    // 注册 GATTS 回调
    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret) 
    {
        ESP_LOGE(GATTS_TABLE_TAG, "GATTS register failed: %x", ret);
        return;
    }
    // 注册 GAP 回调
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret) 
    {
        ESP_LOGE(GATTS_TABLE_TAG, "GAP register failed: %x", ret);
        return;
    }
    // 注册应用程序 (唯一 app_id)
    ret = esp_ble_gatts_app_register(0x55);
    if (ret) 
    {
        ESP_LOGE(GATTS_TABLE_TAG, "App register failed: %x", ret);
        return;
    }
    // 设置 MTU 大小
    esp_ble_gatt_set_local_mtu(500);
}