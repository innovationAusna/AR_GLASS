// text_storage.h

#ifndef TEXT_STORAGE_H
#define TEXT_STORAGE_H

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

// 初始化文本存储模块
void text_storage_init(void);

// 更新存储的文本（自动添加换行符）
void update_text(const char *new_text);

// 发送当前存储的文本（包含换行符）
void send_current_text(void);

#ifdef __cplusplus
}
#endif

#endif // TEXT_STORAGE_H