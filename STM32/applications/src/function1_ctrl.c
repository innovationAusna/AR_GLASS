/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-06-16     caiti       the first version
 */

// function1_ctrl.c

#include <rtthread.h>

#include "function1_ctrl.h"
#include "event.h"
#include "function1_display_thread.h"

#define DBG_TAG "main"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

static struct rt_thread function1_ctrl;
ALIGN(RT_ALIGN_SIZE)
static rt_uint8_t function1_ctrl_stack[FUNCTION1_CTRL_STACK_SIZE];

static void function1_ctrl_entry(void *parameter)
{
    rt_uint32_t recv;
    while (1)
    {
        if (rt_event_recv(&event,
                          EVT_FUNCTION1_START | EVT_FUNCTION1_STOP,
                          RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
                          RT_WAITING_FOREVER,
                          &recv) == RT_EOK)
        {
            if (recv & EVT_FUNCTION1_START)
            {
                // 开始滚动显示
                display_thread_init();
            }
            if (recv & EVT_FUNCTION1_STOP)
            {
                // 停止滚动显示
                rt_thread_detach(&display_thread);
            }
        }
    }
}

int function1_ctrl_init(void)
{
    rt_err_t err = rt_thread_init(&function1_ctrl,
                                  "function1_ctrl",
                                  function1_ctrl_entry,
                                  RT_NULL,
                                  function1_ctrl_stack,
                                  sizeof(function1_ctrl_stack),
                                  FUNCTION1_CTRL_PRIORITY,
                                  FUNCTION1_CTRL_TICK);
    if (err == RT_EOK)
    {
        rt_thread_startup(&function1_ctrl);
        LOG_I("function1_ctrl started");
    }
    else
    {
        LOG_E("Failed to start function1_ctrl: %d", err);
    }
    return err;
}
