/*
 * hiusbc_debugfs_common.h -- Debug Header File for Hisilicon USB Controller.
 *
 * Copyright (c) 2019 Hisilicon Technologies CO., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __HIUSBC_DEBUGFS_COMMON_H
#define __HIUSBC_DEBUGFS_COMMON_H

#include "hiusbc_core.h"
#include "include/drv_log.h"

extern u32 hiusbc_log_level;

#ifdef STATIC_SKIP
#define STATIC
#else
#define STATIC static
#endif

#define HIUSBC_DEBUG_ERR		BIT(0)
#define HIUSBC_DEBUG_SYS		BIT(1)
#define HIUSBC_DEBUG_EP			BIT(2)
#define HIUSBC_DEBUG_CMD		BIT(3)
#define HIUSBC_DEBUG_EVENT		BIT(4)
#define HIUSBC_DEBUG_XFER		BIT(5)
#define HIUSBC_DEBUG_CTRL		BIT(6)
#define HIUSBC_DEBUG_ISOC		BIT(7)
#define HIUSBC_DEBUG_BULK		BIT(8)
#define HIUSBC_DEBUG_INT		BIT(9)
#define HIUSBC_DEBUG_TEMP		BIT(10)
#define HIUSBC_DEBUG_EVT_RING	BIT(11)
#define HIUSBC_DEBUG_XFER_RING	BIT(12)
#define HIUSBC_DEBUG_PERF		BIT(13)
#define HIUSBC_DEBUG_INFO		BIT(14)

/* keep the same with that in xhci.h and usb.h */
#define HIUSBC_XHCI_ERR			BIT(0)
#define HIUSBC_XHCI_TEMP		BIT(10)

#define hiusbc_dbg(level, fmt...)		\
	do {					\
		if ((hiusbc_log_level & (level)) || ((level) == HIUSBC_DEBUG_INFO)) {	\
			if ((level) == HIUSBC_DEBUG_ERR)	\
				drv_err("HIUSBC", fmt);	\
			else							\
				drv_info("HIUSBC", fmt);	\
		}	\
	} while (0)

void hiusbc_clear_xfer_event_counter(struct hiusbc_ep *hep);
void hiusbc_show_xfer_event_counter(struct hiusbc_ep *hep);
void hiusbc_clear_dev_event_counter(struct hiusbc *hiusbc);
void hiusbc_show_dev_event_counter(struct hiusbc *hiusbc);
int hiusbc_debug_knock_ep(struct hiusbc_ep *hep);
void hiusbc_cptest_set_to_cp0(struct hiusbc *hiusbc);
void hiusbc_cptest_next_pattern(struct hiusbc *hiusbc);

#endif /* __HIUSBC_DEBUG_H */
