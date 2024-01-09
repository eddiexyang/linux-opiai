/*
* Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* Description:mdc_v2 means ascend610lite
* Author: huawei
* Create: 2023-10-25
*/

#ifndef PCIE_RC_CTRL_H
#define PCIE_RC_CTRL_H

#ifndef PCIE_RC_UT
#include "pcie_port_init.h"

#define ENABLE  1
#define DISABLE 0

int drv_pcierc_ctrl_uninit(struct platform_device *pdev);
void drv_pcierc_ctrl_init(u32 core_id);
phys_addr_t drv_pcierc_get_reg_base(u32 core_id);
void drv_pcierc_wait_link_up(u32 core_id, u32 port_id, bool *self_heal_flag);
void drv_pcierc_resume_self_heal_handle(void);
#endif
#endif