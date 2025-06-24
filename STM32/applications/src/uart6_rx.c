/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-06-16     caiti       the first version
 */

// uart6_rx.c

#include <stdio.h>
#include <rtthread.h>
#include <board.h>

#include "uart6_rx.h"
#include "event.h"
#include "function1_display_thread.h"
#include "function2_display_thread.h"
#include <packages/ssd1306-latest/include/ssd1306.h>

#define DBG_TAG "uart6_rx"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

// 信息接收字符串
static char information_buf[UART6_RX_BUF_SIZE]  = "";
// 功能选择字符串
static char select_func_buf[FUNCTION_BUF_SIZE]  = "function1";

// 信息接收索引
static volatile rt_size_t information_index = 0;

// 提词器
void function1(char *word_string)
{
    if (strcmp(word_string, "start") == 0)
    {
        /* 恢复滚动线程 */
        rt_event_send(&event, EVT_FUNCTION1_START);
    }
    else if (strcmp(word_string, "stop") == 0)
    {
        /* 暂停滚动线程 */
        rt_event_send(&event, EVT_FUNCTION1_STOP);
    }
    else
    {
        strncpy(display_str, word_string, UART6_RX_BUF_SIZE);
    }
}

// 导航
void function2(char *guide_string)
{
    if ( (strcmp(guide_string, "forward" ) == 0)||
         (strcmp(guide_string, "backward") == 0)||
         (strcmp(guide_string, "right"   ) == 0)||
         (strcmp(guide_string, "left"    ) == 0) )
    {
        strncpy(dirc_string, guide_string, DIRC_LENGTH);
        rt_event_send(&event, EVT_FUNCTION2_DIRC);
    }
    else
    {
        snprintf(dis_string, DIS_LENGTH, "%sm", guide_string);
        rt_event_send(&event, EVT_FUNCTION2_DIS);
    }
}

// 串口接收函数
static rt_err_t uart6_rx_ind(rt_device_t dev, rt_size_t size)
{
    char ch0;
    while (rt_device_read(dev, 0, &ch0, 1) == 1)
    {
        if (ch0 == '\n' || information_index >= UART6_RX_BUF_SIZE - 1)
        {
            information_buf[information_index] = '\0';

            if (strcmp(information_buf, "function1") == 0)
            {
                // 显示function1
                rt_event_send(&event, EVT_FUNCTION1);
                strncpy(select_func_buf, information_buf, FUNCTION_BUF_SIZE);
            }
            else if (strcmp(information_buf, "function2") == 0)
            {
                // 显示function2
                rt_event_send(&event, EVT_FUNCTION2);
                strncpy(select_func_buf, information_buf, FUNCTION_BUF_SIZE);
            }
            else if (strcmp(information_buf, "function3") == 0)
            {
                // 显示function3
                rt_event_send(&event, EVT_FUNCTION3);
                strncpy(select_func_buf, information_buf, FUNCTION_BUF_SIZE);
            }
            else
            {
                if(strcmp(select_func_buf, "function1") == 0)
                {
                    // 提词器
                    function1(information_buf);
                }
                else if(strcmp(select_func_buf, "function2") == 0)
                {
                    // 导航
                    function2(information_buf);
                }
                else if(strcmp(select_func_buf, "function3") == 0)
                {
                    // AI互动
                    function1(information_buf);
                }
                else
                {

                }
            }
            memset(information_buf, 0, sizeof(information_buf));
            information_index = 0;
        }
        else
        {
            information_buf[information_index++] = ch0;
        }
    }
    return RT_EOK;
}

int uart6_rx_init(void)
{
    rt_device_t serial = rt_device_find("uart6");
    if (serial == RT_NULL)
    {
        LOG_E("uart6 device not found");
        return -RT_ERROR;
    }

    struct serial_configure cfg = RT_SERIAL_CONFIG_DEFAULT;
    cfg.baud_rate = BAUD_RATE_115200;
    cfg.data_bits = DATA_BITS_8;
    cfg.stop_bits = STOP_BITS_1;
    cfg.parity    = PARITY_NONE;
    rt_device_control(serial, RT_DEVICE_CTRL_CONFIG, &cfg);

    rt_device_open(serial, RT_DEVICE_FLAG_INT_RX);
    rt_device_set_rx_indicate(serial, uart6_rx_ind);

    return RT_EOK;
}
