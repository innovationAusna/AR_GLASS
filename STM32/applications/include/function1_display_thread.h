/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-06-15     caiti       the first version
 */

// display_thread.h

#ifndef APPLICATIONS_INCLUDE_FUNCTION1_DISPLAY_THREAD_H_
#define APPLICATIONS_INCLUDE_FUNCTION1_DISPLAY_THREAD_H_

#include <rtthread.h>

// 每行最大字符数
#define CHARS_PER_LINE     12
// OLED 可见行数
#define LINES_ON_SCREEN    2
// 字符串长度
#define STRING_LENGTH      512

// 全局字符串
extern char display_str[STRING_LENGTH];

extern struct rt_thread display_thread;

#define DISPLAY_THREAD_STACK_SIZE  2048
#define DISPLAY_THREAD_PRIORITY    10
#define DISPLAY_THREAD_TICK        50

void display_thread_entry(void *parameter);

int display_thread_init(void);

#endif /* APPLICATIONS_INCLUDE_DISPLAY_THREAD_H_ */
