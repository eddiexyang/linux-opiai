/*
 * hiusbc_debugfs.h -- Debugfs Header File for Hisilicon USB Controller.
 *
 * Copyright (c) 2019 Hisilicon Technologies CO., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __HIUSBC_DEBUGFS_H
#define __HIUSBC_DEBUGFS_H
#include <linux/sched.h>
#include "hiusbc_core.h"

#define DEBUGFS_ROOT_NAME_LEN 128
#define hiusbc_fs_err(fmt, ...)     \
    do {                    \
        drv_err("HIUSBC", "[uid=%d] " fmt, __kuid_val(current_uid()), ##__VA_ARGS__);  \
    } while (0)

#define hiusbc_fs_info(fmt, ...)        \
    do {                    \
        drv_info("HIUSBC", "[uid=%d] " fmt, __kuid_val(current_uid()), ##__VA_ARGS__); \
    } while (0)

void hiusbc_debugfs_init(struct hiusbc *hiusbc);
void hiusbc_debugfs_exit(struct hiusbc *hiusbc);
#endif /* __HIUSBC_DEBUGFS_H */
