/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-06-16     caiti       the first version
 */

// event.c

#include <rtdevice.h>

#include "event.h"

#define DBG_TAG "event"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

struct rt_event event;

int event_init(void)
{
    rt_err_t ret;

    /* 初始化事件控制块，FIFO 排队 */
    ret = rt_event_init(&event, "event", RT_IPC_FLAG_FIFO);
    if (ret != RT_EOK)
    {
        LOG_E("event_ctrl init failed: %d", ret);
    }
    else
    {
        LOG_I("event_ctrl initialized");
    }

    return ret;
}
