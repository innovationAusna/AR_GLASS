/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-06-16     caiti       the first version
 */

// function2_display_thread.c

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <string.h>

#include "function2_display_thread.h"
#include "event.h"

#include <packages/ssd1306-latest/include/ssd1306.h>

#define DBG_TAG "display2"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

// 方向字符串
char dirc_string[DIRC_LENGTH] = "forward";
// 距离字符串
char dis_string[DIS_LENGTH] = "0";

/* 静态栈 */
struct rt_thread function2_display_thread;
ALIGN(RT_ALIGN_SIZE)
static rt_uint8_t function2_display_thread_stack[FUNCTION2_DISPLAY_THREAD_STACK_SIZE];

void function2_display_thread_entry(void *parameter)
{
    rt_uint32_t recv;
    while(1)
    {
        if (rt_event_recv(&event,
                          EVT_FUNCTION2_DIRC | EVT_FUNCTION2_DIS,
                          RT_EVENT_FLAG_AND | RT_EVENT_FLAG_CLEAR,
                          RT_WAITING_FOREVER,
                          &recv) == RT_EOK)
        {
            ssd1306_Fill(Black);

            // 距离显示
            ssd1306_SetCursor(25, 20 + 10);
            ssd1306_WriteString(dis_string, Font_7x10, White);

            if(strcmp(dirc_string, "left") == 0)
            {
                // 向左箭头
                ssd1306_Line(5, 24, 1, 30, White);
                ssd1306_Line(2, 30, 14, 30, White);
                ssd1306_Line(5, 36, 1, 30, White);
            }
            else if(strcmp(dirc_string, "right") == 0)
            {
                // 向右箭头
                ssd1306_Line(14, 30, 10, 24, White);
                ssd1306_Line(2, 30, 14, 30, White);
                ssd1306_Line(14, 30, 10, 36, White);
            }
            else if(strcmp(dirc_string, "forward") == 0)
            {
                // 向前箭头
                ssd1306_Line(4, 29, 8, 23, White);
                ssd1306_Line(8, 24, 8, 36, White);
                ssd1306_Line(12, 29, 8, 23, White);
            }
            else
            {
                // 向后箭头
                ssd1306_Line(4, 31, 8, 37, White);
                ssd1306_Line(8, 24, 8, 36, White);
                ssd1306_Line(12, 31, 8, 37, White);
            }

            /* 刷新屏幕 */
            ssd1306_UpdateScreen();
        }
    }
}

int function2_display_thread_init(void)
{
    rt_err_t result = rt_thread_init(&function2_display_thread,
                                     "function2_display_thread",
                                     function2_display_thread_entry,
                                     RT_NULL,
                                     &function2_display_thread_stack[0],
                                     sizeof(function2_display_thread_stack),
                                     FUNCTION2_DISPLAY_THREAD_PRIORITY,
                                     FUNCTION2_DISPLAY_THREAD_TICK);
    if (result == RT_EOK)
    {
        rt_thread_startup(&function2_display_thread);
    }
    else
    {
        LOG_E("scroll_thread init failed: %d", result);
    }
    return result;
}
