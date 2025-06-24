/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-06-16     caiti       the first version
 */

// function1_ctrl.h

#ifndef APPLICATIONS_INCLUDE_FUNCTION1_CTRL_H_
#define APPLICATIONS_INCLUDE_FUNCTION1_CTRL_H_

#include <rtthread.h>

#define FUNCTION1_CTRL_STACK_SIZE  1024
#define FUNCTION1_CTRL_PRIORITY    8
#define FUNCTION1_CTRL_TICK        5

int function1_ctrl_init(void);

#endif /* APPLICATIONS_INCLUDE_FUNCTION1_CTRL_H_ */
