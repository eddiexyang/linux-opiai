/*
 * hiusbc_asic_config.c -- Config for asic USB Controller.
 *
 * Copyright (c) 2019 Hisilicon Technologies CO., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/property.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/kmod.h>
#include "include/securec.h"
#include "hiusbc_asic_config.h"
#include "hiusbc_debugfs_common.h"

static int hiusbc_probe_num = 0;
static struct qos_master_node g_node;
const struct io_arwuser_ctrl g_arwuser_ctrl[USBC_NUM] = {
	{0x3604, 0x3614, 0x3608, 0x3618},
	{0x3624, 0x3634, 0x3628, 0x3638},
	{0x3644, 0x3654, 0x3648, 0x3658},
	{0x3664, 0x3674, 0x3668, 0x3678}
};
struct qos_master_config_type g_qos_record = {
	.type = MASTER_USB,
	.mpamid = 0,
	.qos = 3,
	.pmg = 1,
	.bitmap[0] = 0xf,
	.mode = 0,
};

struct qos_config_record g_qos_cfg_record[QOS_SCH_USB_NUM] = {
	{
		.allow_cfg_read_record.qos_allow_ctrl = QOS_ALLOW_CTRL_READ,
		.allow_cfg_write_record.qos_allow_ctrl = QOS_ALLOW_CTRL_WRITE,
	},
	{
		.allow_cfg_read_record.qos_allow_ctrl = QOS_ALLOW_CTRL_READ,
		.allow_cfg_write_record.qos_allow_ctrl = QOS_ALLOW_CTRL_WRITE,
	}
};

int hiusbc_dev_func_enable(struct hiusbc *hiusbc)
{
	char *usb_id[] = {"0", "1", "2", "3"};
	char *argv[] = {
		"/bin/bash",
		"/var/ascend_usb_config.sh",
		usb_id[hiusbc->usb_id],
		NULL
	};
	char *envp[] = {0, NULL};
	int ret = 0;

	ret = call_usermodehelper("/bin/bash", argv, envp, UMH_WAIT_EXEC);
	if (ret) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "call usermodehelper failed! ret=%d.\n", ret);
		return ret;
	}

	return ret;
}

int hiusbc_gpio_lpmset(struct hiusbc *hiusbc, enum HIUSBC_LPM_STATUS status)
{
	int i, ret = 0;
	u32 gpio_val = 0;

	if (hiusbc->gpio_count <= 0) {
		hiusbc_dbg(HIUSBC_DEBUG_SYS, "the GPIO configuration is not obtained.\n");
		return 0;
	}

	for (i = 0; i < hiusbc->gpio_count; i++) {
		if (hiusbc->current_dr_mode == HIUSBC_DR_MODE_HOST)
			gpio_val = hiusbc->gpio_host_value[i];
		else if (hiusbc->current_dr_mode == HIUSBC_DR_MODE_DEVICE)
			gpio_val = hiusbc->gpio_dev_value[i];

		ret = gpiod_direction_output(
			gpio_to_desc(hiusbc->gpios[i]),
			status == HIUSBC_SUSPEND ?
			hiusbc->gpio_suspend_value[i] : gpio_val);
		if (ret) {
			hiusbc_dbg(HIUSBC_DEBUG_ERR, "gpios value set failed.\n");
			return ret;
		}
	}
	return ret;
}

static int hiusbc_gpio_request(struct hiusbc *hiusbc)
{
	struct device_node *np = hiusbc->dev->of_node;
	int i, ret = 0;
	u32 gpio_val = 0;

	for (i = 0; i < hiusbc->gpio_count; i++) {
		int gpio = of_get_named_gpio(np, "gpios", i);
		if (gpio < 0) {
			hiusbc_dbg(HIUSBC_DEBUG_ERR, "gpio config get failed, i=%d.\n", i);
			return gpio;
		}

		if (hiusbc->support_dr_mode == HIUSBC_DR_MODE_HOST)
			gpio_val = hiusbc->gpio_host_value[i];
		else if (hiusbc->support_dr_mode == HIUSBC_DR_MODE_DEVICE)
			gpio_val = hiusbc->gpio_dev_value[i];

		if (gpio_is_valid(gpio)) {
			ret = devm_gpio_request_one(hiusbc->dev, gpio,
										gpio_val ? GPIOF_OUT_INIT_HIGH :
										GPIOF_OUT_INIT_LOW,
										dev_name(hiusbc->dev));
			if (ret) {
				hiusbc_dbg(HIUSBC_DEBUG_ERR, "gpio request failed, i=%d.\n", i);
				return ret;
			}
			hiusbc->gpios[i] = gpio;
		} else {
			hiusbc_dbg(HIUSBC_DEBUG_ERR, "gpio is invalid.\n");
			return -EINVAL;
		}
	}
	return 0;
}

void hiusbc_gpio_free(struct hiusbc *hiusbc)
{
	devm_kfree(hiusbc->dev, hiusbc->gpios);
	hiusbc->gpios = NULL;
	devm_kfree(hiusbc->dev, hiusbc->gpio_host_value);
	hiusbc->gpio_host_value = NULL;
	devm_kfree(hiusbc->dev, hiusbc->gpio_dev_value);
	hiusbc->gpio_dev_value = NULL;
	devm_kfree(hiusbc->dev, hiusbc->gpio_suspend_value);
	hiusbc->gpio_suspend_value = NULL;
}

static int hiusbc_gpio_alloc(struct hiusbc *hiusbc)
{
	hiusbc->gpios = devm_kmalloc_array(hiusbc->dev, hiusbc->gpio_count,
										sizeof(unsigned), GFP_KERNEL);
	if (hiusbc->gpios == NULL) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "gpios desc array alloc failed.\n");
		return -ENOMEM;
	}

	hiusbc->gpio_host_value =
		devm_kmalloc_array(hiusbc->dev, hiusbc->gpio_count, sizeof(u32), GFP_KERNEL);
	if (hiusbc->gpio_host_value == NULL) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "gpios host value array alloc failed.\n");
		devm_kfree(hiusbc->dev, hiusbc->gpios);
		hiusbc->gpios = NULL;
		return -ENOMEM;
	}

	hiusbc->gpio_dev_value =
		devm_kmalloc_array(hiusbc->dev, hiusbc->gpio_count, sizeof(u32), GFP_KERNEL);
	if (hiusbc->gpio_dev_value == NULL) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "gpios dev value array alloc failed.\n");
		devm_kfree(hiusbc->dev, hiusbc->gpios);
		hiusbc->gpios = NULL;
		devm_kfree(hiusbc->dev, hiusbc->gpio_host_value);
		hiusbc->gpio_host_value = NULL;
		return -ENOMEM;
	}

	hiusbc->gpio_suspend_value =
		devm_kmalloc_array(hiusbc->dev, hiusbc->gpio_count, sizeof(u32), GFP_KERNEL);
	if (hiusbc->gpio_suspend_value == NULL) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "gpios suspend value array alloc failed.\n");
		devm_kfree(hiusbc->dev, hiusbc->gpios);
		hiusbc->gpios = NULL;
		devm_kfree(hiusbc->dev, hiusbc->gpio_host_value);
		hiusbc->gpio_host_value = NULL;
		devm_kfree(hiusbc->dev, hiusbc->gpio_dev_value);
		hiusbc->gpio_dev_value = NULL;
		return -ENOMEM;
	}

	return 0;
}

int hiusbc_gpio_config(struct hiusbc *hiusbc)
{
	int ret = 0;

	/* Not applicable to mode switching scenarios */
	hiusbc->gpio_count = gpiod_count(hiusbc->dev, NULL);
	if (hiusbc->gpio_count <= 0) {
		hiusbc_dbg(HIUSBC_DEBUG_INFO, "hiusbc gpio configuration doesn't exist.\n");
		return 0;
	}

	ret = hiusbc_gpio_alloc(hiusbc);
	if (ret) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "gpios config alloc failed.\n");
		return ret;
	}

	ret = device_property_read_u32_array(hiusbc->dev,
										"gpios-host-value",
										hiusbc->gpio_host_value,
										hiusbc->gpio_count);
	if (ret) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "gpios host value get failed.\n");
		goto err;
	}

	ret = device_property_read_u32_array(hiusbc->dev,
										"gpios-device-value",
										hiusbc->gpio_dev_value,
										hiusbc->gpio_count);
	if (ret) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "gpios dev value get failed.\n");
		goto err;
	}

	ret = device_property_read_u32_array(hiusbc->dev,
										"gpios-suspend-value",
										hiusbc->gpio_suspend_value,
										hiusbc->gpio_count);
	if (ret) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "gpios suspend value get failed.\n");
		goto err;
	}

	ret = hiusbc_gpio_request(hiusbc);
	if (ret) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "gpios config request failed.\n");
		goto err;
	}
	return 0;

err:
	hiusbc_gpio_free(hiusbc);
	return ret;
}

static void hiusbc_arwuser_init(struct hiusbc *hiusbc)
{
	u32 reg;
	u32 aruser_ctrl1_offset = g_arwuser_ctrl[hiusbc->usb_id].aruser_ctrl1_offset;
	u32 awuser_ctrl1_offset = g_arwuser_ctrl[hiusbc->usb_id].awuser_ctrl1_offset;

	//AR AW channel use complete cacheline and not use smmu
	//aruser
	reg = readl(hiusbc->io_subctrl_regs + aruser_ctrl1_offset);
	reg |= BIT(4);
	writel(reg, hiusbc->io_subctrl_regs + aruser_ctrl1_offset);
	reg = readl(hiusbc->io_subctrl_regs + aruser_ctrl1_offset);
	reg &= ~BIT(5);
	writel(reg, hiusbc->io_subctrl_regs + aruser_ctrl1_offset);
	//awuser
	reg = readl(hiusbc->io_subctrl_regs + awuser_ctrl1_offset);
	reg |= BIT(4);
	writel(reg, hiusbc->io_subctrl_regs + awuser_ctrl1_offset);
	reg = readl(hiusbc->io_subctrl_regs + awuser_ctrl1_offset);
	reg &= ~BIT(5);
	writel(reg, hiusbc->io_subctrl_regs + awuser_ctrl1_offset);
	//set oca
	reg = readl(hiusbc->io_subctrl_regs + SC_USB_PORT_OCA_CTRL_OFFSET);
	reg |= BIT(8);
	writel(reg, hiusbc->io_subctrl_regs + SC_USB_PORT_OCA_CTRL_OFFSET);
}

static void hiusbc_clock_reset_init(struct hiusbc* hiusbc)
{
	u32 reg;
	/* 1. disable clkgate */
	reg = readl(hiusbc->io_subctrl_regs + SC_USB_ICG_DIS_OFFSET);
	reg |= BIT(hiusbc->usb_id);
	writel(reg, hiusbc->io_subctrl_regs + SC_USB_ICG_DIS_OFFSET);

	/* 2. reset phy and controller */
	if (hiusbc->usb_id > 0) {
		reg = readl(hiusbc->io_subctrl_regs + SC_USBPHY_RESET_REQ_OFFSET);
		reg |= BIT(hiusbc->usb_id - 1);
		writel(reg, hiusbc->io_subctrl_regs + SC_USBPHY_RESET_REQ_OFFSET);
	}
	reg = readl(hiusbc->io_subctrl_regs + SC_USB_RESET_REQ_OFFSET);
	reg |= BIT(hiusbc->usb_id);
	writel(reg, hiusbc->io_subctrl_regs + SC_USB_RESET_REQ_OFFSET);

	/* 3. enable clkgate */
	reg = readl(hiusbc->io_subctrl_regs + SC_USB_ICG_EN_OFFSET);
	reg |= BIT(hiusbc->usb_id);
	writel(reg, hiusbc->io_subctrl_regs + SC_USB_ICG_EN_OFFSET);

	/* 4. wait 100us */
	udelay(100);

	/* 5. disable clkgate */
	reg = readl(hiusbc->io_subctrl_regs + SC_USB_ICG_DIS_OFFSET);
	reg |= BIT(hiusbc->usb_id);
	writel(reg, hiusbc->io_subctrl_regs + SC_USB_ICG_DIS_OFFSET);

	/* 6. deassert reset */
	reg = readl(hiusbc->io_subctrl_regs + SC_USB_RESET_DREQ_OFFSET);
	reg |= hiusbc_reset_dreq_set(hiusbc->usb_id);
	writel(reg, hiusbc->io_subctrl_regs + SC_USB_RESET_DREQ_OFFSET);

	/* 7. phy deassert reset */
	if (hiusbc->usb_id > 0) {
		reg = readl(hiusbc->io_subctrl_regs + SC_USBPHY_RESET_DREQ_OFFSET);
		reg |= BIT(hiusbc->usb_id - 1);
		writel(reg, hiusbc->io_subctrl_regs + SC_USBPHY_RESET_DREQ_OFFSET);
	}

	/* 8. enable clkgate */
	reg = readl(hiusbc->io_subctrl_regs + SC_USB_ICG_EN_OFFSET);
	reg |= BIT(hiusbc->usb_id);
	writel(reg, hiusbc->io_subctrl_regs + SC_USB_ICG_EN_OFFSET);
}


int hiusbc_set_qos_cfg(int dev_id, const struct qos_master_config_type *cfg)
{
	int i;
	void __iomem *io_subctrl, *lmi;
	u32 reg;
	u32 usbc_base_reg = USBC0_BASE_REG_ADDR;

	if ((cfg->bitmap[0] & 0xf) == 0) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "para bitmap:0x%llx error.\n", cfg->bitmap[0]);
		return DRV_ERROR_PARA_ERROR;
	}

	io_subctrl = ioremap(IO_SUBCTL_REG_BASE, IO_SUBCTL_REG_SIZE);
	if (!io_subctrl) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "io_subctrl map failed.\n");
		return -ENOMEM;
	}
	if (memcpy_s(&g_qos_record, sizeof(struct qos_master_config_type),
					cfg, sizeof(struct qos_master_config_type)) != 0)
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "memcpy_s failed.\n");

	for (i = 0; i < USBC_NUM; i++) {
		if ((cfg->bitmap[0] >> i) & 0x1) {
			//pmg
			reg = readl(io_subctrl + g_arwuser_ctrl[i].aruser_ctrl1_offset);
			reg = (reg & ~HIUSBC_PMG_MASK) | hiusbc_pmg_set(cfg->pmg);
			writel(reg, io_subctrl + g_arwuser_ctrl[i].aruser_ctrl1_offset);
			reg = readl(io_subctrl + g_arwuser_ctrl[i].awuser_ctrl1_offset);
			reg = (reg & ~HIUSBC_PMG_MASK) | hiusbc_pmg_set(cfg->pmg);
			writel(reg, io_subctrl + g_arwuser_ctrl[i].awuser_ctrl1_offset);
			//mpamid
			reg = readl(io_subctrl + g_arwuser_ctrl[i].aruser_ctrl2_offset);
			reg = (reg & ~HIUSBC_PMAPAID_MASK) | hiusbc_pmpaid_set(cfg->mpamid);
			writel(reg, io_subctrl + g_arwuser_ctrl[i].aruser_ctrl2_offset);
			reg = readl(io_subctrl + g_arwuser_ctrl[i].awuser_ctrl2_offset);
			reg = (reg & ~HIUSBC_PMAPAID_MASK) | hiusbc_pmpaid_set(cfg->mpamid);
			writel(reg, io_subctrl + g_arwuser_ctrl[i].awuser_ctrl2_offset);
			//qos
			lmi = ioremap(usbc_base_reg + LMI_BASE_ADDR, USBC_QOS_SIZE);
			if (lmi) {
				reg = readl(lmi + LMI_AXI_QOS_MAP_CFG_OFFSET);
				reg = (reg & ~HIUSB_QOS_MASK) | hiusbc_qos_set(cfg->qos);
				writel(reg, lmi + LMI_AXI_QOS_MAP_CFG_OFFSET);
				iounmap(lmi);
			} else {
				iounmap(io_subctrl);
				hiusbc_dbg(HIUSBC_DEBUG_ERR, "usbc%d lmi map failed.\n", i);
				return -ENOMEM;
			}
		}
		usbc_base_reg += USBC_REG_SIZE;
	}

	iounmap(io_subctrl);
	return 0;
}

int hiusbc_get_qos_cfg(int dev_id, struct qos_master_config_type *cfg)
{
	void __iomem *io_subctrl;
	void __iomem *lmi;
	u32 reg;
	u32 usbc_base_reg = USBC0_BASE_REG_ADDR;
	int i;

	io_subctrl = ioremap(IO_SUBCTL_REG_BASE, IO_SUBCTL_REG_SIZE);
	if (!io_subctrl) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "io_subctrl map failed.\n");
		return -ENOMEM;
	}

	for (i = 0; i < USBC_NUM; i++) {
		if ((cfg->bitmap[0] == 0) || ((cfg->bitmap[0] >> i) & 0x1)) {
			//pmg
			reg = readl(io_subctrl + g_arwuser_ctrl[i].aruser_ctrl1_offset);
			cfg->pmg = hiusbc_pmg_get(reg);
			//mpamid
			reg = readl(io_subctrl + g_arwuser_ctrl[i].aruser_ctrl2_offset);
			cfg->mpamid = hiusbc_pmpaid_get(reg);
			iounmap(io_subctrl);

			lmi = ioremap(usbc_base_reg + LMI_BASE_ADDR, USBC_QOS_SIZE);
			if (lmi) {
				reg = readl(lmi + LMI_AXI_QOS_MAP_CFG_OFFSET);
				cfg->qos = hiusbc_qos_get(reg);
				iounmap(lmi);
			} else {
				hiusbc_dbg(HIUSBC_DEBUG_ERR, "usbc lmi map failed.\n");
				return -ENOMEM;
			}
			return 0;
		}
		usbc_base_reg += USBC_REG_SIZE;
	}

	iounmap(io_subctrl);
	return 0;
}

STATIC int hiusbc_set_allow_cfg_check(int dev_id, const struct qos_allow_config_type *cfg)
{
	if (cfg == NULL) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "Param input invalid. (cfg = NULL)\n");
		return -EINVAL;
	}

	// disable bp
	if (cfg->qos_allow_mode == QOS_ALLOW_MODE_DISABLE)
		return 0;

	// response bp
	if (cfg->qos_allow_mode != QOS_ALLOW_MODE_RESPONSE) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "Param input invalid. (cfg->qos_allow_mode=%u)\n", cfg->qos_allow_mode);
		return -EINVAL;
	}

	// write
	if ((cfg->qos_allow_ctrl != QOS_ALLOW_MODE_DISABLE) &&
		(cfg->qos_allow_ctrl != QOS_ALLOW_MODE_PRODUCE) &&
		(cfg->qos_allow_ctrl != QOS_ALLOW_MODE_RESPONSE)) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "Param input invalid. (cfg->qos_allow_ctrl=%u)\n", cfg->qos_allow_ctrl);
		return -EINVAL;
	}

	// lvl0> lvl1 > lvl2 range: (0, 255]
	if ((cfg->qos_allow_lvl[LVL0] <= cfg->qos_allow_lvl[LVL1]) || (cfg->qos_allow_lvl[LVL1] <= cfg->qos_allow_lvl[LVL2])
		|| (cfg->qos_allow_lvl[LVL0] > QOS_ALLOW_LVL_MAX) || (cfg->qos_allow_lvl[LVL2] <= QOS_ALLOW_LVL_MIN)) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "Param input invalid. (cfg->qos_allow_lvl[%u]=%u, " \
			"cfg->qos_allow_lvl[%u]=%u, cfg->qos_allow_lvl[%u]= %u)\n",
			LVL0, cfg->qos_allow_lvl[LVL0], LVL1, cfg->qos_allow_lvl[LVL1],
			LVL2, cfg->qos_allow_lvl[LVL2]);

		return -EINVAL;
	}

	return 0;
}

STATIC int hiusbc_set_rw_allow_cfg(const struct qos_allow_config_type *cfg, u32 reg_addr)
{
	u32 reg;
	void __iomem *io_subctrl;

	io_subctrl = ioremap(reg_addr, IO_SUBCTL_REG_SIZE);
	if (!io_subctrl) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "io_subctrl map failed.\n");
		return -ENOMEM;
	}

	reg = readl(io_subctrl);
	if (cfg->qos_allow_mode == QOS_ALLOW_MODE_DISABLE) { // disable this function
		reg = ((reg & ~HIUSBC_OSTD_SEL_MASK) | hiusbc_outstanding_set(0)); // bit 25 26 set 0
	} else {
		// cfg->qos_allow_mode == 2
		reg = ((reg & ~HIUSBC_ALLOW_CFG_MASK) | (hiusbc_outstanding_set(2) |
				hiusbc_allow_1vl0_set(cfg->qos_allow_lvl[LVL0]) |
				hiusbc_allow_1vl1_set(cfg->qos_allow_lvl[LVL1]) |
				hiusbc_allow_1vl2_set(cfg->qos_allow_lvl[LVL2])));
	}

	writel(reg, io_subctrl);

	iounmap(io_subctrl);
	return 0;
}

int hiusbc_set_allow_cfg(int dev_id, const struct qos_allow_config_type *cfg)
{
	if (hiusbc_set_allow_cfg_check(dev_id, cfg) != 0) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "Invalid argument.\n");
		return DRV_ERROR_PARA_ERROR;
	}

	switch (cfg->qos_allow_ctrl) {
	case 0: // all
		if ((hiusbc_set_rw_allow_cfg(cfg, QOS_ALLOW_CFG_RD1_REG) != 0) ||
			(hiusbc_set_rw_allow_cfg(cfg, QOS_ALLOW_CFG_RD2_REG) != 0) ||
			(hiusbc_set_rw_allow_cfg(cfg, QOS_ALLOW_CFG_WR1_REG) != 0) ||
			(hiusbc_set_rw_allow_cfg(cfg, QOS_ALLOW_CFG_WR2_REG) != 0))
			return -EINVAL;
		break;
	case 1: // read
		if ((hiusbc_set_rw_allow_cfg(cfg, QOS_ALLOW_CFG_RD1_REG) != 0) ||
			(hiusbc_set_rw_allow_cfg(cfg, QOS_ALLOW_CFG_RD2_REG) != 0))
			return -EINVAL;
		break;
	case 2: // write
		if ((hiusbc_set_rw_allow_cfg(cfg, QOS_ALLOW_CFG_WR1_REG) != 0) ||
			(hiusbc_set_rw_allow_cfg(cfg, QOS_ALLOW_CFG_WR2_REG) != 0))
			return -EINVAL;
		break;

	default:
		break;
	}

	return 0;
}

static int hiusbc_write_allow_cfg_reg(const struct qos_allow_config_type *cfg, u32 reg_addr)
{
	u32 reg;
	void __iomem *io_subctrl;

	io_subctrl = ioremap(reg_addr, IO_SUBCTL_REG_SIZE);
	if (io_subctrl == NULL) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "io_subctrl map failed.\n");
		return -ENOMEM;
	}

	reg = readl(io_subctrl);
	reg = ((reg & ~HIUSBC_ALLOW_CFG_MASK) | (hiusbc_outstanding_set(cfg->qos_allow_mode) |
			hiusbc_allow_1vl0_set(cfg->qos_allow_lvl[LVL0]) |
			hiusbc_allow_1vl1_set(cfg->qos_allow_lvl[LVL1]) |
			hiusbc_allow_1vl2_set(cfg->qos_allow_lvl[LVL2])));

	writel(reg, io_subctrl);

	iounmap(io_subctrl);
	return 0;
}


static int hiusbc_get_rw_allow_cfg(struct qos_allow_config_type *cfg, u32 reg_addr)
{
	u32 reg;
	void __iomem *io_subctrl;

	io_subctrl = ioremap(reg_addr, IO_SUBCTL_REG_SIZE);
	if (!io_subctrl) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "io_subctrl map failed.\n");
		return -ENOMEM;
	}

	reg = readl(io_subctrl);
	cfg->qos_allow_lvl[LVL0] = hiusbc_allow_1vl0_get(reg);
	cfg->qos_allow_lvl[LVL1] = hiusbc_allow_1vl1_get(reg);
	cfg->qos_allow_lvl[LVL2] = hiusbc_allow_1vl2_get(reg);

	iounmap(io_subctrl);
	return 0;
}

int hiusbc_get_allow_cfg(int dev_id, struct qos_allow_config_type *cfg)
{
	int ret = 0;

	if (cfg == NULL) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "Param input invalid. (cfg = NULL)\n");
		return -EINVAL;
	}

	if ((cfg->qos_allow_ctrl != QOS_ALLOW_CTRL_READ) && (cfg->qos_allow_ctrl != QOS_ALLOW_CTRL_WRITE)) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "Invalid argument. (cfg->qos_allow_ctrl=%u)\n", cfg->qos_allow_ctrl);
		return DRV_ERROR_PARA_ERROR;
	}

	if (cfg->qos_allow_ctrl == QOS_ALLOW_CTRL_READ) {
		ret = hiusbc_get_rw_allow_cfg(cfg, QOS_ALLOW_CFG_RD1_REG);
	} else {
		ret = hiusbc_get_rw_allow_cfg(cfg, QOS_ALLOW_CFG_WR1_REG);
	}

	return ret;
}

static int hiusbc_get_rw_allow_mode(struct qos_allow_config_type *cfg, u32 reg_addr)
{
	u32 reg;
	void __iomem *io_subctrl;

	io_subctrl = ioremap(reg_addr, IO_SUBCTL_REG_SIZE);
	if (io_subctrl == NULL) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "io_subctrl map failed.\n");
		return -ENOMEM;
	}

	reg = readl(io_subctrl);
	cfg->qos_allow_mode = hiusbc_outstanding_get(reg);

	iounmap(io_subctrl);
	return 0;
}

STATIC int hiusbc_set_otsd_cfg_check(int dev_id, const struct qos_otsd_config_type *cfg)
{
	if (cfg == NULL) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "Param input invalid. (cfg = NULL)\n");
		return -EINVAL;
	}

	if (cfg->otsd_mode == QOS_OTSD_MODE_DISABLE)
		return 0;

	if ((cfg->otsd_mode != QOS_OTSD_MODE_RW_MERGE) && (cfg->otsd_mode != QOS_OTSD_MODE_RW_NOT_MERGE)) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "Param input invalid. (cfg->otsd_mode=%u)\n", cfg->otsd_mode);
		return -EINVAL;
	}

	// lvl0 / lvl1 range: (0, 255]
	if ((cfg->otsd_lvl[0] > QOS_OTSD_LVL_MAX) || (cfg->otsd_lvl[0] <= QOS_OTSD_LVL_MIN)) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "Param input invalid. (cfg->otsd_lvl[0]=%u)\n", cfg->otsd_lvl[LVL0]);
		return -EINVAL;
	}

	if ((cfg->otsd_mode == QOS_OTSD_MODE_RW_NOT_MERGE) &&
		((cfg->otsd_lvl[1] > QOS_OTSD_LVL_MAX) || (cfg->otsd_lvl[1] <= QOS_OTSD_LVL_MIN))) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "Param input invalid. (cfg->otsd_lvl[1]=%u)\n", cfg->otsd_lvl[1]);
		return -EINVAL;
	}

	return 0;
}

STATIC int hiusbc_set_otsd_cfg_rw(const struct qos_otsd_config_type *cfg, unsigned int addr)
{
	u32 reg;
	void __iomem *io_subctrl;

	io_subctrl = ioremap(addr, IO_SUBCTL_REG_SIZE);
	if (!io_subctrl) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "io_subctrl map failed.\n");
		return -ENOMEM;
	}

	reg = readl(io_subctrl);
	if (cfg->otsd_mode == QOS_OTSD_MODE_DISABLE) { // bit 8 17 set 0
		reg = ((reg & ~HIUSBC_OTSD_CFG_EN_MASK) | hiusbc_otsd_rd_en_set(0) | hiusbc_otsd_wr_en_set(0));
	} else if (cfg->otsd_mode == QOS_OTSD_MODE_RW_MERGE) {
		reg = ((reg & ~HIUSBC_OTSD_CFG_MASK) | hiusbc_otsd_rd_en_set(1) | hiusbc_otsd_wr_en_set(1) |
				hiusbc_otsd_rd_set(cfg->otsd_lvl[LVL0]) | hiusbc_otsd_wr_set(cfg->otsd_lvl[LVL0]));
	} else {
		reg = ((reg & ~HIUSBC_OTSD_CFG_MASK) | hiusbc_otsd_rd_en_set(1) | hiusbc_otsd_wr_en_set(1) |
				hiusbc_otsd_rd_set(cfg->otsd_lvl[LVL0]) | hiusbc_otsd_wr_set(cfg->otsd_lvl[LVL1]));
	}
	writel(reg, io_subctrl);
	iounmap(io_subctrl);

	return 0;
}

int hiusbc_set_otsd_cfg(int dev_id, const struct qos_otsd_config_type *cfg)
{
	if (hiusbc_set_otsd_cfg_check(dev_id, cfg) != 0) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "Invalid argument.\n");
		return DRV_ERROR_PARA_ERROR;
	}

	if ((hiusbc_set_otsd_cfg_rw(cfg, QOS_OTSD_CFG_RW1_REG) != 0) ||
		(hiusbc_set_otsd_cfg_rw(cfg, QOS_OTSD_CFG_RW2_REG) != 0)) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "Set otsd cfg failed.\n");
		return -ENOMEM;
	}

	return 0;
}

static int hiusbc_write_otsd_cfg_reg(const struct qos_otsd_config_type *cfg, u32 reg_addr)
{
	u32 reg;
	void __iomem *io_subctrl;
	io_subctrl = ioremap(reg_addr, IO_SUBCTL_REG_SIZE);
	if (io_subctrl == NULL) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "io_subctrl map failed.\n");
		return -ENOMEM;
	}

	reg = readl(io_subctrl);
	reg = ((reg & ~HIUSBC_OTSD_CFG_MASK) | hiusbc_otsd_rd_en_set(cfg->otsd_mode) |
			hiusbc_otsd_wr_en_set(cfg->otsd_mode) | hiusbc_otsd_rd_set(cfg->otsd_lvl[LVL0]) |
			hiusbc_otsd_wr_set(cfg->otsd_lvl[LVL1]));

	writel(reg, io_subctrl);

	iounmap(io_subctrl);
	return 0;
}

int hiusbc_get_otsd_cfg(int dev_id, struct qos_otsd_config_type *cfg)
{
	u32 reg;
	void __iomem *io_subctrl;

	// two registers have same value, select one to get otsd cfg
	io_subctrl = ioremap(QOS_OTSD_CFG_RW1_REG, IO_SUBCTL_REG_SIZE);
	if (!io_subctrl) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "io_subctrl map failed.\n");
		return -ENOMEM;
	}
	reg = readl(io_subctrl);

	cfg->otsd_lvl[LVL0] = hiusbc_otsd_rd_get(reg);
	cfg->otsd_lvl[LVL1] = hiusbc_otsd_wr_get(reg);

	iounmap(io_subctrl);

	return 0;
}

static int hiusbc_get_rw_otsd_mode(struct qos_otsd_config_type *cfg, u32 reg_addr)
{
	// IO_SUBCTL_RW_OTSD_CFG_REG
	u32 reg;
	void __iomem *io_subctrl;

	if (cfg == NULL) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "Param input invalid. (cfg = NULL)\n");
		return -EINVAL;
	}

	io_subctrl = ioremap(reg_addr, IO_SUBCTL_REG_SIZE);
	if (io_subctrl == NULL) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "io_subctrl map failed.\n");
		return -ENOMEM;
	}
	reg = readl(io_subctrl);
	cfg->otsd_mode = hiusbc_otsd_rd_en_get(reg);

	iounmap(io_subctrl);
	return 0;
}


void hiusbc_save_qos_cfg(void)
{
	int i;

	for (i = 0; i < QOS_SCH_USB_NUM; i++) {
		hiusbc_get_allow_cfg(0, &g_qos_cfg_record[i].allow_cfg_read_record);
		hiusbc_get_allow_cfg(0, &g_qos_cfg_record[i].allow_cfg_write_record);
		hiusbc_get_otsd_cfg(0, &g_qos_cfg_record[i].otsd_cfg_record);
	}

	hiusbc_get_rw_allow_mode(&g_qos_cfg_record[0].allow_cfg_read_record, QOS_ALLOW_CFG_RD1_REG);
	hiusbc_get_rw_allow_mode(&g_qos_cfg_record[1].allow_cfg_read_record, QOS_ALLOW_CFG_RD2_REG);
	hiusbc_get_rw_allow_mode(&g_qos_cfg_record[0].allow_cfg_write_record, QOS_ALLOW_CFG_WR1_REG);
	hiusbc_get_rw_allow_mode(&g_qos_cfg_record[1].allow_cfg_write_record, QOS_ALLOW_CFG_WR2_REG);
	hiusbc_get_rw_otsd_mode(&g_qos_cfg_record[0].otsd_cfg_record, QOS_OTSD_CFG_RW1_REG);
	hiusbc_get_rw_otsd_mode(&g_qos_cfg_record[1].otsd_cfg_record, QOS_OTSD_CFG_RW2_REG);

	return;
}

int hiusbc_restore_qos_cfg(void)
{
	if ((hiusbc_write_allow_cfg_reg(&g_qos_cfg_record[0].allow_cfg_read_record, QOS_ALLOW_CFG_RD1_REG) != 0) ||
		(hiusbc_write_allow_cfg_reg(&g_qos_cfg_record[1].allow_cfg_read_record, QOS_ALLOW_CFG_RD2_REG) != 0) ||
		(hiusbc_write_allow_cfg_reg(&g_qos_cfg_record[0].allow_cfg_write_record, QOS_ALLOW_CFG_WR1_REG) != 0) ||
		(hiusbc_write_allow_cfg_reg(&g_qos_cfg_record[1].allow_cfg_write_record, QOS_ALLOW_CFG_WR2_REG) != 0)) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "Restore allow cfg failed!\n");
		return -EINVAL;
	}

	if ((hiusbc_write_otsd_cfg_reg(&g_qos_cfg_record[0].otsd_cfg_record, QOS_OTSD_CFG_RW1_REG) != 0) ||
		(hiusbc_write_otsd_cfg_reg(&g_qos_cfg_record[1].otsd_cfg_record, QOS_OTSD_CFG_RW2_REG) != 0)) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "Restore otsd cfg failed!\n");
		return -EINVAL;
	}

	return 0;
}

static int hiusbc_axi_qos_init(struct hiusbc *hiusbc)
{
	int ret = 0;

	if (strcmp(g_node.name, HIUSBC_QOS_NAME)) {
		ret = strcpy_s(g_node.name, MAX_QOS_MASTER_NODE_NAME_LEN, HIUSBC_QOS_NAME);
		if (ret) {
			hiusbc_dbg(HIUSBC_DEBUG_ERR, "Call strcpy_s failed. (ret=%d)\n", ret);
			return ret;
		}
		g_node.cfg.type = MASTER_USB;
		g_node.set = hiusbc_set_qos_cfg;
		g_node.get = hiusbc_get_qos_cfg;
		g_node.set_allow = hiusbc_set_allow_cfg;
		g_node.get_allow = hiusbc_get_allow_cfg;
		g_node.set_otsd = hiusbc_set_otsd_cfg;
		g_node.get_otsd = hiusbc_get_otsd_cfg;
		//ret = hal_kernel_qos_node_register(&g_node);
		//if (ret) {
		//	g_node.name[0] = '\0';
		//	hiusbc_dbg(HIUSBC_DEBUG_ERR, "hiusbc qos register failed.\n");
		//	return ret;
		//}
	}
	++hiusbc_probe_num;
	return ret;
}

void hiusbc_qos_exit(void)
{
	--hiusbc_probe_num;
	//if ((hiusbc_probe_num == 0) && (strcmp(g_node.name, HIUSBC_QOS_NAME) == 0))
	//	(void)hal_kernel_qos_node_unregister(&g_node);
}

static void hiusbc_pcs_init(struct hiusbc *hiusbc)
{
	u32 reg;
	u32 val;

	reg = readl(hiusbc->pcs_regs + USB_CSR3_OFFSET);
	reg &= ~USB_CSR3_SPEED_MASK;
	writel(reg, hiusbc->pcs_regs + USB_CSR3_OFFSET);
	reg = readl(hiusbc->pcs_regs + USB_CSR12_OFFSET);
	reg &= ~USB_CSR12_MASK;
	writel(reg, hiusbc->pcs_regs + USB_CSR12_OFFSET);
	reg = readl(hiusbc->pcs_regs + USB_CSR3_OFFSET);
	reg &= ~USB_CSR3_FREQ_MASK;
	writel(reg, hiusbc->pcs_regs + USB_CSR3_OFFSET);
	val = 0x700;
	reg = readl(hiusbc->pcs_regs + USB_CSR34_OFFSET);
	reg = (reg & ~USB_CSR_FFE_MASK) | val;
	writel(reg, hiusbc->pcs_regs + USB_CSR34_OFFSET);
	val = 0x55c0;
	reg = readl(hiusbc->pcs_regs + USB_CSR35_OFFSET);
	reg = (reg & ~USB_CSR_FFE_MASK) | val;
	writel(reg, hiusbc->pcs_regs + USB_CSR35_OFFSET);
	val = 0x7540;
	reg = readl(hiusbc->pcs_regs + USB_CSR36_OFFSET);
	reg = (reg & ~USB_CSR_FFE_MASK) | val;
	writel(reg, hiusbc->pcs_regs + USB_CSR36_OFFSET);
	reg = readl(hiusbc->pcs_regs + USB_CSR1_OFFSET);
	reg |= BIT(15) | BIT(11) | BIT(9) | BIT(7);
	writel(reg, hiusbc->pcs_regs + USB_CSR1_OFFSET);
	reg = readl(hiusbc->pcs_regs + USB_CSR12_OFFSET);
	reg &= ~BIT(23);
	writel(reg, hiusbc->pcs_regs + USB_CSR12_OFFSET);
	usleep_range(200, 300);
	val = 0xffff;
	reg = readl(hiusbc->pcs_regs + USB_CSR5_OFFSET);
	reg |= val;
	writel(reg, hiusbc->pcs_regs + USB_CSR5_OFFSET);
	// mask pcs int
	reg = readl(hiusbc->pcs_regs + USB_CSR30);
	reg |= val;
	writel(reg, hiusbc->pcs_regs + USB_CSR30);
}

void hiusbc_constraints_init(struct hiusbc *hiusbc)
{
	u32 reg;
	hiusbc_dbg(HIUSBC_DEBUG_SYS, "+\n");

	/* LMI 0x17008 */
	reg = hiusbc_readl(hiusbc->lmi_regs, LMI_AXI_CACHE_CFG_OFFSET);
	reg |= 0xf;
	hiusbc_writel(reg, hiusbc->lmi_regs, LMI_AXI_CACHE_CFG_OFFSET);
	/* LMI 0x17000 */
	reg = hiusbc_readl(hiusbc->lmi_regs, LMI_AXI_AWLEN_CFG_OFFSET);
	reg = (reg & ~CSR_LMI_LEN_CTRL_MASK) | 0x1;
	hiusbc_writel(reg, hiusbc->lmi_regs, LMI_AXI_AWLEN_CFG_OFFSET);
	hiusbc_dbg(HIUSBC_DEBUG_SYS, "-\n");
}

STATIC int usb_phy_cfg_adjust(struct hiusbc *hiusbc)
{
	struct device *dev = hiusbc->dev;
	struct fwnode_handle *phy;
	int addr_cnt, mask_cnt, value_cnt, i;
	u32 addr[MAX_CFG_NUM] = {0}, mask[MAX_CFG_NUM] = {0}, value[MAX_CFG_NUM] = {0};
	u32 reg_val, phy_size;
	u64 para64[RES_NUM] = {0};
	void __iomem *phy_addr;
	int ret = 0;

	phy = device_get_named_child_node(dev, "phy");
	if (phy == NULL)
		return 0;
	if (fwnode_property_read_u64_array(phy, "phy_base", para64, RES_NUM) != 0) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "get phy base fail\n");
		return -EINVAL;
	}
	phy_addr = ioremap(para64[0], (u32)para64[1]);
	phy_size = (u32)para64[1];
	if (phy_addr == NULL)
		return -ENOMEM;
	addr_cnt = fwnode_property_count_u32(phy, "cfg_off");
	mask_cnt = fwnode_property_count_u32(phy, "cfg_mask");
	value_cnt = fwnode_property_count_u32(phy, "cfg_value");
	if (addr_cnt != mask_cnt || addr_cnt != value_cnt) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "phy cfg num is not equal\n");
		ret = -EINVAL;
		goto out;
	}
	if (addr_cnt > MAX_CFG_NUM || addr_cnt <= 0) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "phy cfg num is out of range\n");
		ret = -EINVAL;
		goto out;
	}
	(void)fwnode_property_read_u32_array(phy, "cfg_off", addr, addr_cnt);
	(void)fwnode_property_read_u32_array(phy, "cfg_mask", mask, mask_cnt);
	(void)fwnode_property_read_u32_array(phy, "cfg_value", value, value_cnt);
	for (i = 0; i < addr_cnt; i++) {
		if (addr[i] >= phy_size || addr[i] % 4 != 0) {
			hiusbc_dbg(HIUSBC_DEBUG_ERR, "phy addr is illegal\n");
			ret = -EINVAL;
			goto out;
		}
		reg_val = readl(phy_addr + addr[i]) & (~mask[i]);
		reg_val |= value[i] & mask[i];
		writel(reg_val, phy_addr + addr[i]);
	}

out:
	iounmap(phy_addr);
	return ret;
}

int hiusbc_parse_dts(struct hiusbc *hiusbc)
{
	int ret = 0;
	struct device *dev = hiusbc->dev;
	u64 para[RES_NUM] = {0};

	if (device_property_read_u32(dev, "u2-only", &hiusbc->u2_only))
		hiusbc->u2_only = 0;
	hiusbc_dbg(HIUSBC_DEBUG_INFO, "u2 only = %d\n", hiusbc->u2_only);

	ret = device_property_read_u32(dev, "usb-id", &hiusbc->usb_id);
	if (ret) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "dev get usb-id failed.\n");
		return ret;
	}

	ret = device_property_read_u64_array(dev, "io-subctrl-base", para, RES_NUM);
	if (ret) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "dev get io-subctrl-base failed.\n");
		return ret;
	}

	hiusbc->io_subctrl_regs = devm_ioremap(dev, para[0], (u32)para[1]);
	if (!hiusbc->io_subctrl_regs)
		return -ENOMEM;

	ret = device_property_read_u64_array(dev, "pcs-base", para, RES_NUM);
	if (ret) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "dev get pcs-base failed.\n");
		return ret;
	}

	hiusbc->pcs_regs = devm_ioremap(dev, para[0], (u32)para[1]);
	if (!hiusbc->pcs_regs) {
		devm_iounmap(hiusbc->dev, hiusbc->io_subctrl_regs);
		hiusbc->io_subctrl_regs = NULL;
		return -ENOMEM;
	}

	return 0;
}

int hiusbc_soc_init(struct hiusbc *hiusbc)
{
	int ret = 0;

	hiusbc_arwuser_init(hiusbc);

	hiusbc_clock_reset_init(hiusbc);

	hiusbc_pcs_init(hiusbc);

	//U2 eye
	ret = usb_phy_cfg_adjust(hiusbc);
	if (ret) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "asic phy config failed.\n");
		return ret;
	}

	return 0;
}

int hiusbc_asic_init(struct hiusbc *hiusbc)
{
	int ret = 0;

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "+\n");
	ret = hiusbc_parse_dts(hiusbc);
	if (ret) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "asic parse dts failed.\n");
		return ret;
	}

	ret = hiusbc_soc_init(hiusbc);
	if (ret) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "asic soc init failed.\n");
		return ret;
	}

	ret = hiusbc_axi_qos_init(hiusbc);
	if (ret) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "asic qos initial failed.\n");
		return ret;
	}
	hiusbc_dbg(HIUSBC_DEBUG_SYS, "-\n");
	return ret;
}

void hiusbc_asic_exit(struct hiusbc *hiusbc)
{
	hiusbc_qos_exit();
}
