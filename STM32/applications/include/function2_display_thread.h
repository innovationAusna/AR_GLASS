/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-06-16     caiti       the first version
 */

// function2_display_thread.h

#ifndef APPLICATIONS_INCLUDE_FUNCTION2_DISPLAY_THREAD_H_
#define APPLICATIONS_INCLUDE_FUNCTION2_DISPLAY_THREAD_H_

#include <rtthread.h>

// 方向字符串长度
#define DIRC_LENGTH      8
// 距离字符串长度
#define DIS_LENGTH       8

// 方向字符串
extern char dirc_string[DIRC_LENGTH];
// 距离字符串
extern char dis_string[DIS_LENGTH];

extern struct rt_thread function2_display_thread;

#define FUNCTION2_DISPLAY_THREAD_STACK_SIZE  1024
#define FUNCTION2_DISPLAY_THREAD_PRIORITY    9
#define FUNCTION2_DISPLAY_THREAD_TICK        5

void function2_display_thread_entry(void *parameter);

int function2_display_thread_init(void);

#endif /* APPLICATIONS_INCLUDE_FUNCTION2_DISPLAY_THREAD_H_ */
