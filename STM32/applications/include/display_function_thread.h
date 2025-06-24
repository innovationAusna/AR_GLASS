/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-06-16     caiti       the first version
 */

// display_function_thread.h

#ifndef APPLICATIONS_INCLUDE_DISPLAY_FUNCTION_THREAD_H_
#define APPLICATIONS_INCLUDE_DISPLAY_FUNCTION_THREAD_H_

extern struct rt_thread display_function_thread;

#define DISPLAY_FUNCTION_THREAD_STACK_SIZE  1024
#define DISPLAY_FUNCTION_THREAD_PRIORITY    20
#define DISPLAY_FUNCTION_THREAD_TICK        5

void display_function_thread_entry(void *parameter);

int display_function_thread_init(void);

#endif /* APPLICATIONS_INCLUDE_DISPLAY_FUNCTION_THREAD_H_ */
