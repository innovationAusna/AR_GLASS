/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-06-16     caiti       the first version
 */

// display_function_thread.c

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <string.h>

#include "display_function_thread.h"
#include "event.h"

#include <packages/ssd1306-latest/include/ssd1306.h>

#define DBG_TAG "display_function"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

/* 静态栈 */
struct rt_thread display_function_thread;
ALIGN(RT_ALIGN_SIZE)
static rt_uint8_t display_function_thread_stack[DISPLAY_FUNCTION_THREAD_STACK_SIZE];

// 显示功能
void function_display(char *function)
{
    ssd1306_Fill(Black);

    // 显示文字
    ssd1306_SetCursor(2, 20 + 10);
    ssd1306_WriteString(function, Font_7x10, White);

    /* 刷新屏幕 */
    ssd1306_UpdateScreen();
}

void display_function_thread_entry(void *parameter)
{
    rt_uint32_t recv;
    while(1)
    {
        if (rt_event_recv(&event,
                          EVT_FUNCTION1 | EVT_FUNCTION2 | EVT_FUNCTION3,
                          RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
                          RT_WAITING_FOREVER,
                          &recv) == RT_EOK)
        {
            if (recv & EVT_FUNCTION1)
            {
                function_display("function1");
            }
            else if (recv & EVT_FUNCTION2)
            {
                function_display("function2");
            }
            else
            {
                function_display("function3");
            }
        }
    }
}

int display_function_thread_init(void)
{
    rt_err_t result = rt_thread_init(&display_function_thread,
                                     "display_function_thread",
                                     display_function_thread_entry,
                                     RT_NULL,
                                     &display_function_thread_stack[0],
                                     sizeof(display_function_thread_stack),
                                     DISPLAY_FUNCTION_THREAD_PRIORITY,
                                     DISPLAY_FUNCTION_THREAD_TICK);
    if (result == RT_EOK)
    {
        rt_thread_startup(&display_function_thread);
    }
    else
    {
        LOG_E("scroll_thread init failed: %d", result);
    }
    return result;
}
