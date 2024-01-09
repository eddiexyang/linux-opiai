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
#include <linux/of_address.h>
#include <linux/of_pci.h>
#include <linux/pci-ecam.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/securec.h>

#include "pcie_rc_drv.h"
#include "pcie_rc_ctrl.h"

#ifndef PCIE_RC_UT

struct rc_ltssm ltssm_rc_cfg_def_18 = {
    .ltssm_cfg = {
        .max_sup_width = CFG_WX4,
        .max_lk_spd = GEN1,
        .target_speed = GEN1,
        .reset_level = 0,
        .glb_cfg = {
            .port_en = 1,
            .port_mode = PORT_RP,
            .lane_num = GBL_WX4,
        },
        .dl_cfg = {
            .init_fc_send_en_valid = 0,
            .init_fc_send_en_val = 1,
        },
        .equ_cfg = {
            .adj_en = 1,
            .test_en = 1,
            .reg_phy_eq_fb_sel = 0,

            .reg_8g_hilink_mode_en = 0x1d,
            .gen3_eq_rcv_lock_af_phase13_ctle_apt_en = 0,
            .rcv_speed_pull_up_rx_preset_en = 0,
            .gen3_eq_phase23_ctle_apt_en = 1,
            .rcv_lock_hold_rate = 0,
            .reg_switch_eq_mode_mask = 1,
            .gen4_eq_rcv_lock_af_phase13_ctle_apt_en = 0,
            .gen4_eq_phase23_ctle_apt_en = 1,
            .gen3_eq_phase01_ctle_apt_en = 1,
            .gen4_eq_phase01_ctle_apt_en = 1,
            .gen3_eq_phase23_ffe_ctle_adj_en = 0,
            .gen4_eq_phase23_ffe_ctle_adj_en = 0,
            .reg_16g_hilink_mode_en = 0x1d,
            .reg_16g_ds_use_rx_preset_en = 1,

            /*
            .reg_8g_eq_fix_lp_tx_preset = 0x7,
            */
            .reg_8g_eq_fix_lp_tx_preset = 0x9,
            .reg_16g_eq_fix_lp_tx_preset = 0x9,

            /*
            .link_control3_register01 = 0x7070707,
            */
            // .link_control3_register01 = 0x9090909,
            // .link_control3_register23 = 0x9090909,
            // .link_control3_register45 = 0x9090909,
            // .link_control3_register67 = 0x9090909,
            // .link_control3_register89 = 0x9090909,
            // .link_control3_register1011 = 0x9090909,
            // .link_control3_register1213 = 0x9090909,
            // .link_control3_register1415 = 0x9090909,
            // .gen4_phy_cap_reg20 = 0x99999999,
            // .gen4_phy_cap_reg24 = 0x99999999,
            // .gen4_phy_cap_reg28 = 0x99999999,
            // .gen4_phy_cap_reg2c = 0x99999999,
            .cfg_phase23_hold_timer = 0x2c,
            .cfg_phase0_hold_timer = 0x14,
            .cfg_rcv_lock_hold_timer = 0x2c,
        },
        .mac_cfg = {
            .auto_speed_change_en = 1,
            .first_auto_speed_change_en = 1,
            .disable_enter_compliance = 1,
        },
        .tgt_lk_spd = GEN4,

        .tgt_lk_width = CFG_WX8,
    },
};
STATIC void pcie_rc_interrupt_cfg(u32 core_id)
{
    /********close MSIx interrupt Mask***********/
    // AP_IOB_RX_COM_REG.AM_MSI_MSIX_CTRL.msi_msix_enable, only msi_ctrl_idx=0 use
    drv_pcie_ap_write(core_id, PCIE_AP_IOB_RX_COM_REG_BASE, PCIE_AP_IOB_RX_COM_REG_AM_MSI_MSIX_CTRL_REG, DISABLE);
}

STATIC u32 pcie_rc_get_dlinit_linkup_status(u32 core_id, u32 port_id)
{
    return drv_pcie_get_dlinit_linkup_status(core_id, port_id);
}

STATIC u32 pcie_rc_get_ltssm_status(u32 core_id, u32 port_id)
{
    return drv_pcie_get_ltssm_status(core_id, port_id);
}

void drv_pcierc_wait_link_up(u32 core_id, u32 port_id, bool *self_heal_flag)
{
    u32 status;
    u32 ltssm_st;

    do {
        status = pcie_rc_get_dlinit_linkup_status(core_id, port_id);
        ltssm_st = pcie_rc_get_ltssm_status(core_id, port_id);
        msleep(PCIe_DL_LINK_UP_WAIT_TIME);
    } while ((status != PCIE_DL_UP_INT_STATUS_ACTIVE) || (ltssm_st != PCIE_MAC_LTSSM_ST_L0));

    drv_pcie_rc_info("PCIe DL int link up status is 0x%x.(core_id=%u)\n", status, core_id);
    return;
}

int drv_pcierc_ctrl_uninit(struct platform_device *pdev)
{
    u32 core_id;
    u32 port = 0;
    int ret;

    ret = of_property_read_u32(pdev->dev.of_node, "pcie-core-id", &core_id);
    if ((ret < 0) || (core_id >= PCIE_CORE_NUM)) {
        drv_snapshot_bootdot_set(PCIE_MODULE_ID, PCIERC_SUSPEND_GET_CORE_ID_FAIL);
        drv_pcie_rc_err("Failed to get pcie-core-id. (ret=%d; core_id=%u)\n", ret, core_id);
        return -EINVAL;
    }

    if (pcie_rc_get_apb_base(core_id) == NULL) {
        drv_snapshot_bootdot_set(PCIE_MODULE_ID, PCIERC_SUSPEND_GET_APB_BASE_FAIL);
        drv_pcie_rc_err("apb_base is null. (core_id=%u)\n", core_id);
        return -EINVAL;
    }

    pcie_rc_ctrl_lock();
    drv_pcie_rc_info("PCIE RC ctrl uninit start. (core=%u)\n", core_id);

    /* disable tx atu */
    drv_pcie_tx_atu_disable(core_id, port, 0);
    /* disable interrupt */
    pcie_rc_interrupt_cfg(core_id);
    // set PCIE Perst low, bios not set

    drv_pcierc_apb_uninit(core_id);

    drv_pcie_rc_info("PCIE RC ctrl uninit end. (core=%u)\n", core_id);
    pcie_rc_ctrl_unlock();

    return 0;
}

STATIC void pcie_rc_establish_link(struct rc_ltssm* ltssm, u32 port_id)
{
    if (ltssm == NULL) {
        drv_pcie_rc_err("input ltssm is null.\n");
        return;
    }

    drv_pcie_subctrl_dreset();
    drv_pcie_establish_link(&ltssm->ltssm_cfg, port_id);
}

/* correspond to dtsi config */
STATIC void pcie_rc_tx_atu_config(u32 port_id)
{
    u32 core_id = PCIE_CORE_ID;
    u32 atu_id;

    // TX ATU0
    atu_id = 0;
    drv_pcie_ap_write(core_id, PCIE_AP_IOB_TX_REG_BASE,
                      PCIE_AP_IOB_TX_REG_IOB_TXATU_CONTROL_2(atu_id), 0x40U);
    drv_pcie_ap_write(core_id, PCIE_AP_IOB_TX_REG_BASE,
                      PCIE_AP_IOB_TX_REG_IOB_TXATU_REGION_SIZE(atu_id), 0x0); // size
    drv_pcie_ap_write(core_id, PCIE_AP_IOB_TX_REG_BASE,
                      PCIE_AP_IOB_TX_REG_IOB_TXATU_BASE_ADDR_L(atu_id), 0x0); // source addrl
    drv_pcie_ap_write(core_id, PCIE_AP_IOB_TX_REG_BASE,
                      PCIE_AP_IOB_TX_REG_IOB_TXATU_BASE_ADDR_H(atu_id), 0x40); // source addrh
    drv_pcie_ap_write(core_id, PCIE_AP_IOB_TX_REG_BASE,
                      PCIE_AP_IOB_TX_REG_IOB_TXATU_TAR_ADDR_L(atu_id), 0x0); // source addrl
    drv_pcie_ap_write(core_id, PCIE_AP_IOB_TX_REG_BASE,
                      PCIE_AP_IOB_TX_REG_IOB_TXATU_TAR_ADDR_H(atu_id), 0x40); // source addrh
    drv_pcie_ap_write(core_id, PCIE_AP_IOB_TX_REG_BASE,
                      PCIE_AP_IOB_TX_REG_IOB_TXATU_CONTROL_0(atu_id), 0x1); // enable
    // TX ATU2
    atu_id += 2;
    drv_pcie_ap_write(core_id, PCIE_AP_IOB_TX_REG_BASE,
                      PCIE_AP_IOB_TX_REG_IOB_TXATU_CONTROL_2(atu_id), 0x0U);
    drv_pcie_ap_write(core_id, PCIE_AP_IOB_TX_REG_BASE,
                      PCIE_AP_IOB_TX_REG_IOB_TXATU_REGION_SIZE(atu_id), 0x8000000U); // size
    drv_pcie_ap_write(core_id, PCIE_AP_IOB_TX_REG_BASE,
                      PCIE_AP_IOB_TX_REG_IOB_TXATU_BASE_ADDR_L(atu_id), 0xB0000000U); // source addrl
    drv_pcie_ap_write(core_id, PCIE_AP_IOB_TX_REG_BASE,
                      PCIE_AP_IOB_TX_REG_IOB_TXATU_BASE_ADDR_H(atu_id), 0x0); // source addrh
    drv_pcie_ap_write(core_id, PCIE_AP_IOB_TX_REG_BASE,
                      PCIE_AP_IOB_TX_REG_IOB_TXATU_TAR_ADDR_L(atu_id), 0xB0000000U); // source addrl
    drv_pcie_ap_write(core_id, PCIE_AP_IOB_TX_REG_BASE,
                      PCIE_AP_IOB_TX_REG_IOB_TXATU_TAR_ADDR_H(atu_id), 0x0); // source addrh
    drv_pcie_ap_write(core_id, PCIE_AP_IOB_TX_REG_BASE,
                      PCIE_AP_IOB_TX_REG_IOB_TXATU_CONTROL_0(atu_id), 0x1); // enable
}

STATIC void pcie_rc_pf_init(u32 port_id, u32 pf_num)
{
    u32 core_id = PCIE_CORE_ID;

    drv_pcie_ap_write(core_id, PCIE_AP_IOB_TX_REG_BASE,
                      PCIE_AP_IOB_TX_REG_IOB_ECAM_CONTROL_REG, 0x0);
    drv_pcie_ap_write(core_id, PCIE_AP_IOB_TX_REG_BASE,
                      PCIE_AP_IOB_TX_REG_IOB_ECAM_BASE_ADDR_L_REG, 0xD0000000U);
    drv_pcie_ap_write(core_id, PCIE_AP_IOB_TX_REG_BASE,
                      PCIE_AP_IOB_TX_REG_IOB_ECAM_BASE_ADDR_H_REG, 0);
    drv_pcie_ap_write(core_id, PCIE_AP_IOB_RX_COM_REG_BASE,
                      PCIE_AP_IOB_RX_COM_REG_AM_MSI_MSIX_ADDR_HIGH_REG, 0); // 0x80 * 0
    drv_pcie_ap_write(core_id, PCIE_AP_IOB_RX_COM_REG_BASE,
                      PCIE_AP_IOB_RX_COM_REG_AM_MSI_MSIX_ADDR_LOW_REG, 0x87010040);
    drv_pcie_ap_write(core_id, PCIE_AP_IOB_RX_COM_REG_BASE,
                      PCIE_AP_IOB_RX_COM_REG_AM_MSI_MSIX_CTRL_REG, 0x1);

    pcie_rc_tx_atu_config(port_id);
}
void drv_pcierc_ctrl_init(u32 core_id)
{
    u32 port_id = 8;
    u32 pf_num = PF_NUM_1;
    // in case multi core init at the same time, some REGs is public
    pcie_rc_ctrl_lock();

    drv_pcie_rc_info("PCIe RC ctrl init start. (core=%u)\n", core_id);
    // port init
    pcie_rc_establish_link(&ltssm_rc_cfg_def_18, port_id);

    // pf init, from bios
    pcie_rc_pf_init(port_id, pf_num);

    // Port EN
    drv_pcieep_set_port_en(port_id, 1);
    pcie_rc_ctrl_unlock();
    return;
}

void drv_pcierc_resume_self_heal_handle(void)
{
    return;
}

phys_addr_t drv_pcierc_get_reg_base(u32 core_id)
{
    (void)core_id;
    return (phys_addr_t)(PCIE_BASE_ADDR);
}
#else

int drv_pcierc_ctrl_init(void)
{
    return 0;
}
#endif