/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-06-16     caiti       the first version
 */

// display_thread.c

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <string.h>

#include "function1_display_thread.h"

#include <packages/ssd1306-latest/include/ssd1306.h>

#define DBG_TAG "display1"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

char display_str[STRING_LENGTH] = "";

/* 静态栈 */
struct rt_thread display_thread;
ALIGN(RT_ALIGN_SIZE)
static rt_uint8_t display_thread_stack[DISPLAY_THREAD_STACK_SIZE];

/* scroll_thread线程入口函数：不断滚动显示 long_text */
void display_thread_entry(void *parameter)
{
    int len = strlen(display_str);
    int total_lines = (len + CHARS_PER_LINE - 1) / CHARS_PER_LINE;  // 向上取整
    int offset = 0;

    ssd1306_Fill(Black);

    /* 6. 为每行准备一个临时缓冲区，最多 CHARS_PER_LINE + 1 （\0） */
    char line_buf[CHARS_PER_LINE + 1];

    while (1)
    {
       /* 8. 每次循环，先清屏 */
       ssd1306_Fill(Black);

       /* 9. 连续画三行（屏幕可见行），行号 = offset + i */
       for (int i = 0; i < LINES_ON_SCREEN; i++)
       {
           int line_no = offset + i;

           if (line_no < total_lines)
           {
               /* 10. 计算这一行在原字符串中的起始位置 */
               int start_idx = line_no * CHARS_PER_LINE;
               int remain = len - start_idx;
               int copy_len = (remain >= CHARS_PER_LINE) ? CHARS_PER_LINE : remain;

               /* 11. 把这一段拷贝到临时缓冲区，并添加 '\0' */
               memset(line_buf, '\0', sizeof(line_buf));
               memcpy(line_buf, &display_str[start_idx], copy_len);
               line_buf[copy_len] = '\0';

               /* 12. 在 OLED 第 i 行的位置写入文本 */
               /*     假设我们想从 y = 20 + i * 10 像素开始画，字体高度 8 像素 */
               ssd1306_SetCursor(2, 20 + i * 10);
               ssd1306_WriteString(line_buf, Font_7x10, White);
           }
           else
           {
               /* 13. 如果已超出总行数，画空行（可选） */
               ssd1306_SetCursor(2, 20 + i * 10);
               ssd1306_WriteString("                ", Font_7x10, White);
           }
       }

       /* 14. 刷新屏幕 */
       ssd1306_UpdateScreen();

       /* 15. 延时 2 秒，再滚动 */
       rt_thread_mdelay(2000);

       /* 16. 偏移 +1，如果超出范围就回到 0 */
       offset++;
       if (offset > total_lines - LINES_ON_SCREEN)
       {
           offset = 0;
       }
    }
}

int display_thread_init(void)
{
    rt_err_t result = rt_thread_init(&display_thread,
                                     "display_thread",
                                     display_thread_entry,
                                     RT_NULL,
                                     &display_thread_stack[0],
                                     sizeof(display_thread_stack),
                                     DISPLAY_THREAD_PRIORITY,
                                     DISPLAY_THREAD_TICK);
    if (result == RT_EOK)
    {
        rt_thread_startup(&display_thread);
    }
    else
    {
        LOG_E("scroll_thread init failed: %d", result);
    }
    return result;
}
