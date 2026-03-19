/*
 * hiusbc_asic_config.h -- -- Config for asic USB Controller.
 *
 * Copyright (c) 2019 Hisilicon Technologies CO., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __HIUSBC_ASIC_CONFIG_H
#define __HIUSBC_ASIC_CONFIG_H
#include "include/init_reg_define_miniv3.h"
#include "include/ascend_kernel_hal.h"
#include "hiusbc_core.h"

#define MAX_CFG_NUM 16
#define RES_NUM 2
#define SH_SIZE 128
struct io_arwuser_ctrl {
	u32 aruser_ctrl1_offset;
	u32 awuser_ctrl1_offset;
	u32 aruser_ctrl2_offset;
	u32 awuser_ctrl2_offset;
};

struct qos_config_record {
	struct qos_allow_config_type allow_cfg_read_record;
	struct qos_allow_config_type allow_cfg_write_record;
	struct qos_otsd_config_type otsd_cfg_record;
};

enum HIUSBC_LPM_STATUS {
	HIUSBC_SUSPEND,
	HIUSBC_RESUME
};

int hiusbc_asic_init(struct hiusbc *hiusbc);
int hiusbc_soc_init(struct hiusbc *hiusbc);
void hiusbc_asic_exit(struct hiusbc *hiusbc);
void hiusbc_qos_exit(void);
int hiusbc_get_qos_cfg(int dev_id, struct qos_master_config_type *cfg);
int hiusbc_set_qos_cfg(int dev_id, const struct qos_master_config_type *cfg);
int hiusbc_set_allow_cfg(int dev_id, const struct qos_allow_config_type *cfg);
int hiusbc_get_allow_cfg(int dev_id, struct qos_allow_config_type *cfg);
int hiusbc_set_otsd_cfg(int dev_id, const struct qos_otsd_config_type *cfg);
int hiusbc_get_otsd_cfg(int dev_id, struct qos_otsd_config_type *cfg);
void hiusbc_save_qos_cfg(void);
int hiusbc_restore_qos_cfg(void);
void hiusbc_constraints_init(struct hiusbc *hiusbc);
int hiusbc_gpio_setup(struct hiusbc *hiusbc);
int hiusbc_parse_dts(struct hiusbc *hiusbc);
int hiusbc_gpio_lpmset(struct hiusbc *hiusbc, enum HIUSBC_LPM_STATUS status);
int hiusbc_dev_func_enable(struct hiusbc *hiusbc);
int hiusbc_gpio_config(struct hiusbc *hiusbc);
void hiusbc_gpio_free(struct hiusbc *hiusbc);
#endif
