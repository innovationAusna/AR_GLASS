/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-06-16     caiti       the first version
 */

// uart6_rx.h

#ifndef APPLICATIONS_INCLUDE_UART6_RX_H_
#define APPLICATIONS_INCLUDE_UART6_RX_H_

#include <rtthread.h>
#include <rtdevice.h>

#define FUNCTION_BUF_SIZE     10
#define UART6_RX_BUF_SIZE     512

int uart6_rx_init(void);

#endif /* APPLICATIONS_INCLUDE_UART6_RX_H_ */
