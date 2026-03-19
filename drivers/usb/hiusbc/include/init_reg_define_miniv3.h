/*
 * init_reg_define_miniv3.h -- Register List for USB Controller.
 *
 * Copyright (c) 2019 Hisilicon Technologies CO., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __INIT_REG_DEFINE_MINIV3_H
#define __INIT_REG_DEFINE_MINIV3_H

#define USBC0_BASE_REG_ADDR 0xA5000000
#define USBC_REG_SIZE 0x80000
#define USBC_NUM 4
#define USBC_QOS_SIZE 0x10

//For IO_SUBCTRL
#define IO_SUBCTL_REG_BASE 0xA0120000
#define IO_SUBCTL_REG_SIZE 0x10000
#define SC_USB_ICG_EN_OFFSET 0x388
#define SC_USB_ICG_DIS_OFFSET 0x38c
#define SC_USB_RESET_REQ_OFFSET 0x438
#define SC_USB_RESET_DREQ_OFFSET 0x43C
#define SC_USBPHY_RESET_REQ_OFFSET 0x440
#define SC_USBPHY_RESET_DREQ_OFFSET 0x444
#define SC_USB_PORT_OCA_CTRL_OFFSET 0x3690
#define hiusbc_reset_dreq_set(x) (BIT(x) | BIT((x) + 4) | BIT((x) + 8))

//For QOS
#define IO_SUBCTL_QOS_SCH_REG_BASE 0xA0100000
#define SC_USB_CTRL1_OTSD_OFFSET 0x2d8
#define SC_USB_CTRL2_OTSD_OFFSET 0x2dc

#define QOS_ALLOW_CFG_RD1_REG 0xA010038C
#define QOS_ALLOW_CFG_WR1_REG 0xA01003AC
#define QOS_ALLOW_CFG_RD2_REG 0xA0100390
#define QOS_ALLOW_CFG_WR2_REG 0xA01003b0
#define QOS_OTSD_CFG_RW1_REG 0xA01002D8
#define QOS_OTSD_CFG_RW2_REG 0xA01002DC

#define hiusbc_outstanding_set(x) ((x) << 25)
#define hiusbc_allow_1vl0_set(x) (x)
#define hiusbc_allow_1vl1_set(x) ((x) << 8)
#define hiusbc_allow_1vl2_set(x) ((x) << 16)
#define hiusbc_outstanding_get(x) (((x) >> 25) & 0x3)
#define hiusbc_allow_1vl0_get(x) ((x) & 0xFF)
#define hiusbc_allow_1vl1_get(x) (((x) >> 8) & 0xFF)
#define hiusbc_allow_1vl2_get(x) (((x) >> 16) & 0xFF)

#define hiusbc_otsd_rd_en_set(x) ((x) << 17)
#define hiusbc_otsd_wr_en_set(x) ((x) << 8)
#define hiusbc_otsd_rd_set(x) ((x) << 9)
#define hiusbc_otsd_wr_set(x) (x)
#define hiusbc_otsd_rd_en_get(x) (((x) >> 17) & 0x1)
#define hiusbc_otsd_wr_en_get(x) (((x) >> 8) & 0x1)
#define hiusbc_otsd_rd_get(x) (((x) >> 9) & 0xFF)
#define hiusbc_otsd_wr_get(x) ((x) & 0xFF)

#define HIUSBC_OSTD_SEL_MASK 0x6000000
#define HIUSBC_ALLOW_CFG_MASK 0x6FFFFFF

#define HIUSBC_OTSD_CFG_EN_MASK 0x20100
#define HIUSBC_OTSD_CFG_MASK 0x3FFFF

#define QOS_ALLOW_CTRL_ALL   0
#define QOS_ALLOW_CTRL_READ  1
#define QOS_ALLOW_CTRL_WRITE 2

#define QOS_ALLOW_MODE_DISABLE   0
#define QOS_ALLOW_MODE_PRODUCE   1
#define QOS_ALLOW_MODE_RESPONSE  2

#define QOS_ALLOW_LVL_MAX 255
#define QOS_ALLOW_LVL_MIN 0

#define QOS_OTSD_MODE_DISABLE      0
#define QOS_OTSD_MODE_RW_MERGE     1
#define QOS_OTSD_MODE_RW_NOT_MERGE 2

#define QOS_OTSD_LVL_MAX 255
#define QOS_OTSD_LVL_MIN 0

#define QOS_SCH_USB_NUM 2

#define LVL0 0
#define LVL1 1
#define LVL2 2

#define HIUSBC_PMG_MASK 0x60000
#define hiusbc_pmg_set(x) (((x) & 0x3) << 17)
#define hiusbc_pmg_get(x) (((x) & HIUSBC_PMG_MASK) >> 17)
#define HIUSBC_PMAPAID_MASK 0x3FC000
#define hiusbc_pmpaid_set(x) (((x) & 0x7f) << 14)
#define hiusbc_pmpaid_get(x) (((x) & HIUSBC_PMAPAID_MASK) >> 14)
#define HIUSB_QOS_MASK 0xF
#define hiusbc_qos_set(x) ((x) & 0xf)
#define hiusbc_qos_get(x) ((x) & HIUSB_QOS_MASK)
#define HIUSBC_QOS_NAME "HIUSBC"

//For PCS
#define USB_CSR1_OFFSET 0x4
#define USB_CSR3_OFFSET 0xC
#define USB_CSR3_SPEED_MASK 0x700
#define USB_CSR3_FREQ_MASK 0x38
#define USB_CSR5_OFFSET 0x14
#define USB_CSR12_OFFSET 0x30
#define USB_CSR12_MASK 0x7E0000
#define USB_CSR34_OFFSET 0x88
#define USB_CSR35_OFFSET 0x8C
#define USB_CSR36_OFFSET 0x90
#define USB_CSR_FFE_MASK 0x3FFF
#define USB_CSR30 0x78
#endif
