/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-06-16     caiti       the first version
 */

// event.h

#ifndef APPLICATIONS_INCLUDE_EVENT_H_
#define APPLICATIONS_INCLUDE_EVENT_H_

#include <rtthread.h>

// 提词器开始/停止滚动
#define EVT_FUNCTION1_START    (1U << 0)
#define EVT_FUNCTION1_STOP     (1U << 1)

// 导航方向/距离是否已接受到
#define EVT_FUNCTION2_DIRC     (1U << 2)
#define EVT_FUNCTION2_DIS      (1U << 3)

// 开启提词器/导航功能
#define EVT_FUNCTION1          (1U << 4)
#define EVT_FUNCTION2          (1U << 5)
#define EVT_FUNCTION3          (1U << 6)

extern struct rt_event event;

int event_init(void);

#endif /* APPLICATIONS_INCLUDE_EVENT_H_ */
