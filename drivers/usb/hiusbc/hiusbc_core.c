/*
 * hiusbc_core.c -- Driver for Hisilicon USB Controller.
 *
 * Copyright (c) 2019 Hisilicon Technologies CO., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/property.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/string.h>
#include "hiusbc_debugfs_common.h"
#include "hiusbc_drd.h"
#include "hiusbc_debugfs.h"
#include "hiusbc_event.h"
#include "hiusbc_gadget.h"
#include "hiusbc_host.h"
#include "hiusbc_core.h"
#include "hiusbc_asic_config.h"
#ifdef CONFIG_USB_HIUSBCV100_BUGFIX_PATCH
#include <trace/hooks/usbxhci.h>
#include <linux/usb.h>
#include <linux/usb/ch9.h>
#include <linux/usb/hcd.h>
#include "xhci.h"
#endif

struct hiusbc *g_hiusbcs[USBC_NUM];
extern struct qos_master_config_type g_qos_record;
extern struct qos_config_record* g_qos_cfg_record;
struct mutex g_mode_mutex;
int hiusbc_init_phy(struct hiusbc *hiusbc)
{
	u32 reg;
	hiusbc_dbg(HIUSBC_DEBUG_SYS, "+\n");
	if (hiusbc->u2_only) {
		reg = hiusbc_readl(hiusbc->com_regs,
			PHY_SEL_MODE_OFFSET);
		reg &= ~PHY_SEL_MODE_MASK;
		reg |= hiusbc_phy_set(HIUSBC_PHY_SET_U2_ONLY);
		hiusbc_writel(reg, hiusbc->com_regs,
			PHY_SEL_MODE_OFFSET);

		reg = hiusbc_readl(hiusbc->com_regs,
			U3_CONTINUE_MODE_OFFSET);
		reg |= USBC_U2_EN_MASK;
		reg &= ~USBC_U3_EN_MASK;
		hiusbc_writel(reg, hiusbc->com_regs,
			U3_CONTINUE_MODE_OFFSET);
	}
	//usb0 only support u3
	if (hiusbc->usb_id == 0) {
		reg = hiusbc_readl(hiusbc->com_regs,
			PHY_SEL_MODE_OFFSET);
		reg &= ~PHY_SEL_MODE_MASK;
		reg |= hiusbc_phy_set(HIUSBC_PHY_SET_U3_ONLY);
		hiusbc_writel(reg, hiusbc->com_regs,
			PHY_SEL_MODE_OFFSET);

		reg = hiusbc_readl(hiusbc->com_regs,
			U3_CONTINUE_MODE_OFFSET);
		reg |= USBC_U3_EN_MASK;
		reg &= ~USBC_U2_EN_MASK;
		hiusbc_writel(reg, hiusbc->com_regs,
			U3_CONTINUE_MODE_OFFSET);
	}
	hiusbc_dbg(HIUSBC_DEBUG_SYS, "-\n");
	return 0;
}

int hiusbc_get_irq(struct hiusbc *hiusbc)
{
	struct platform_device *hiusbc_pdev = to_platform_device(hiusbc->dev);
	int irq;

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "+\n");
	irq = platform_get_irq(hiusbc_pdev, 0);
	if (irq > 0)
		goto out;

	if (irq != -EPROBE_DEFER)
		hiusbc_dbg(HIUSBC_DEBUG_SYS, "missing irq\n");

	if (!irq)
		irq = -EINVAL;

out:
	hiusbc_dbg(HIUSBC_DEBUG_SYS, "- irq = %d\n", irq);
	return irq;
}

int hiusbc_reset_controller(struct hiusbc *hiusbc)
{
	u32 reg;
	u32 drd_mode_backup;
	u32 timeout = 1000;
	int ret = 0;

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "+\n");

	/* we need to switch to device mode before dev_rst. */
	drd_mode_backup = hiusbc_readl(hiusbc->com_regs,
	DRD_MODE_OFFSET);
	if (hiusbc_get_drd_mode(drd_mode_backup) == HIUSBC_DRD_MODE_HOST)
		hiusbc_writel(
			hiusbc_drd_mode(HIUSBC_DRD_MODE_DEVICE),
			hiusbc->com_regs, DRD_MODE_OFFSET);

	udelay(100);
	reg = hiusbc_readl(hiusbc->dev_regs,
		DEV_RST_OFFSET);
	reg |= DEV_RST_MASK;
	hiusbc_writel(reg, hiusbc->dev_regs,
		DEV_RST_OFFSET);
	udelay(200);

	do {
		reg = hiusbc_readl(hiusbc->dev_regs,
			DEV_RST_OFFSET);
		reg &= DEV_RST_MASK;
		udelay(1);
	} while (reg && --timeout);

	if (reg && !timeout) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "phy done status 0x%x\n",
								hiusbc_readl(hiusbc->com_regs,
								PHY_DONE_STATUS_OFFSET));
		dev_err(hiusbc->dev, "Timeout to reset controller!\n");
		ret = -ETIMEDOUT;
	}

	if (hiusbc_get_drd_mode(drd_mode_backup) == HIUSBC_DRD_MODE_HOST)
		hiusbc_writel(drd_mode_backup,
			hiusbc->com_regs, DRD_MODE_OFFSET);

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "-\n");

	return ret;
}

void hiusbc_set_speed(struct hiusbc *hiusbc,
		enum usb_device_speed speed)
{
	bool u3_en = true;
	bool hs_en = true;
	bool ssp_en = false;
	u32 reg;

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "+ speed = %d\n", speed);

	switch (speed) {
	case USB_SPEED_LOW:
	case USB_SPEED_FULL:
		/* fall-through */
		hs_en = false;
	case USB_SPEED_HIGH:
		/* fall-through */
		u3_en = false;
		break;

	case USB_SPEED_SUPER:
	case USB_SPEED_SUPER_PLUS:
		/* fall-through */
		break;

	default:
		dev_err(hiusbc->dev, "invalid speed = %d.\n", speed);
	}

	if (hiusbc->u2_only)
		u3_en = false;

	reg = hiusbc_readl(hiusbc->com_regs,
	U3_CONTINUE_MODE_OFFSET);
	if (u3_en)
		reg |= USBC_U3_EN_MASK;
	else
		reg &= ~USBC_U3_EN_MASK;
	hiusbc_writel(reg, hiusbc->com_regs,
	U3_CONTINUE_MODE_OFFSET);

	reg = hiusbc_readl(hiusbc->u2_piu_regs,
		CSR_DEV_PRTRST_HS_HANDSHAKE_EN_OFFSET);
	if (hs_en)
		reg |= CSR_DEV_PRTRST_HS_HANDSHAKE_EN_MASK;
	else
		reg &= ~CSR_DEV_PRTRST_HS_HANDSHAKE_EN_MASK;
	hiusbc_writel(reg, hiusbc->u2_piu_regs,
		CSR_DEV_PRTRST_HS_HANDSHAKE_EN_OFFSET);

	reg = hiusbc_readl(hiusbc->u3_link_regs,
		CSR_TARGET_SPEED_OFFSET);
	reg &= ~CSR_TARGET_SPEED_MASK;
	if (ssp_en)
		reg |= (1 << CSR_TARGET_SPEED_SHIFT);
	hiusbc_writel(reg, hiusbc->u3_link_regs,
		CSR_TARGET_SPEED_OFFSET);

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "-\n");
}

STATIC int hiusbc_init_dr_mode(struct hiusbc *hiusbc)
{
	int ret = 0;

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "+\n");

	hiusbc->current_dr_mode = HIUSBC_DR_MODE_UNKNOWN;

	switch (hiusbc->support_dr_mode) {
	case HIUSBC_DR_MODE_HOST:
	case HIUSBC_DR_MODE_DEVICE:
		hiusbc_set_mode(hiusbc, hiusbc->support_dr_mode);
		break;

	case HIUSBC_DR_MODE_BOTH:
		ret = hiusbc_drd_init(hiusbc);
		break;

	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

STATIC void hiusbc_exit_current_mode(struct hiusbc *hiusbc)
{
	unsigned long flags;

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "+\n");

	switch (hiusbc->current_dr_mode) {
	case HIUSBC_DR_MODE_DEVICE:
		hiusbc_gadget_exit(hiusbc);
		break;
	case HIUSBC_DR_MODE_HOST:
		hiusbc_host_exit(hiusbc);
		break;
	default:
		break;
	}

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->current_dr_mode = HIUSBC_DR_MODE_UNKNOWN;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

static void hiusbc_host_config_init(struct hiusbc *hiusbc)
{
	u32 reg;
	/* config trpu block mode */
	reg = hiusbc_readl(hiusbc->host_regs,
		TRPU_HOST_CTRL_MODE_OFFSET);
	reg &= ~TRPU_HOST_BLOCK_MODE_MASK;
	hiusbc_writel(reg, hiusbc->host_regs,
		TRPU_HOST_CTRL_MODE_OFFSET);

	/* config host perf mode */
	hiusbc_writel(0x6, hiusbc->host_regs,
		TRPU_HOST_SEP_MBS_OFFSET);

	hiusbc_writel(0x53d, hiusbc->host_regs,
		FS_MS_PBYTE_NUM_OFFSET);

	/* config host respond doorbell time */
	hiusbc_writel(0x5, hiusbc->host_regs,
		TRPU_HOST_DB_ITVL_OFFSET);
}

static int hiusbc_soft_rst_switch_clk(struct hiusbc *hiusbc)
{
	int ret = 0;
	u32 reg;

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "+\n");

	/* switch clk */
	reg = hiusbc_readl(hiusbc->hub_regs,
		USBC_SOFT_RST_OFFSET);
	reg |= (1 << RST_SOFT_TRPU_DEV_N_SHIFT);
	hiusbc_writel(reg, hiusbc->hub_regs,
		USBC_SOFT_RST_OFFSET);

	ret = hiusbc_reset_controller(hiusbc);
	if (ret) {
		dev_err(hiusbc->dev, "failed to reset controlle! ret = %d\n", ret);
		goto exit;
	}

	hiusbc_writel(hiusbc_drd_mode(HIUSBC_DRD_MODE_DEVICE),
		hiusbc->com_regs, DRD_MODE_OFFSET);

	reg = hiusbc_readl(hiusbc->hub_regs,
		USBC_SOFT_RST_OFFSET);
	reg &= ~RST_SOFT_TRPU_HOST_N_MASK;
	hiusbc_writel(reg, hiusbc->hub_regs,
		USBC_SOFT_RST_OFFSET);

exit:
	hiusbc_dbg(HIUSBC_DEBUG_SYS, "-\n");
	return ret;
}

static void hiusbc_enter_desired_mode(struct hiusbc *hiusbc)
{
	unsigned long flags;
	int ret = 0;
	u32 reg;

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "+\n");

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->current_dr_mode = hiusbc->desired_dr_mode;
	spin_unlock_irqrestore(&hiusbc->lock, flags);

	hiusbc_constraints_init(hiusbc);
	hiusbc_init_phy(hiusbc);

	switch (hiusbc->desired_dr_mode) {
	case HIUSBC_DR_MODE_DEVICE:
		ret = hiusbc_run_stop(hiusbc, 0);
		if (ret) {
			dev_err(hiusbc->dev, "failed to stop controller!\n");
			break;
		}

		ret = hiusbc_soft_rst_switch_clk(hiusbc);
		if (ret) {
			dev_err(hiusbc->dev, "failed to rst after switch clk!\n");
			break;
		}

		ret = hiusbc_gadget_init(hiusbc);
		if (ret) {
			dev_err(hiusbc->dev, "failed to init device mode!\n");
			break;
		}

		ret = hiusbc_dev_func_enable(hiusbc);
		if (ret) {
			hiusbc_gadget_exit(hiusbc);
			dev_err(hiusbc->dev, "udc fs enable failed!\n");
		}
		break;
	case HIUSBC_DR_MODE_HOST:
		ret = hiusbc_run_stop(hiusbc, 0);
		if (ret < 0) {
			dev_err(hiusbc->dev, "failed to stop controller!\n");
			break;
		}

		/* switch clk */
		reg = hiusbc_readl(hiusbc->hub_regs,
			USBC_SOFT_RST_OFFSET);
		reg |= (1 << RST_SOFT_TRPU_HOST_N_SHIFT);
		hiusbc_writel(reg, hiusbc->hub_regs,
			USBC_SOFT_RST_OFFSET);

		hiusbc_host_config_init(hiusbc);

		ret = hiusbc_reset_controller(hiusbc);
		if (ret) {
			dev_err(hiusbc->dev, "failed to reset controller!\n");
			break;
		}

		hiusbc_writel(hiusbc_drd_mode(HIUSBC_DRD_MODE_HOST),
			hiusbc->com_regs, DRD_MODE_OFFSET);

		reg = hiusbc_readl(hiusbc->hub_regs,
			USBC_SOFT_RST_OFFSET);
		reg &= ~RST_SOFT_TRPU_DEV_N_MASK;
		hiusbc_writel(reg, hiusbc->hub_regs,
			USBC_SOFT_RST_OFFSET);

		ret = hiusbc_host_init(hiusbc);
		if (ret)
			dev_err(hiusbc->dev, "failed to init host mode!\n");

		break;
	default:
		break;
	}

	if (ret) {
		spin_lock_irqsave(&hiusbc->lock, flags);
		hiusbc->current_dr_mode = HIUSBC_DR_MODE_UNKNOWN;
		spin_unlock_irqrestore(&hiusbc->lock, flags);
		dev_err(hiusbc->dev, "Set current_dr_mode to _UNKNOWN!\n");
	}
}

STATIC void hiusbc_exit_dr_mode(struct hiusbc *hiusbc)
{
	hiusbc_dbg(HIUSBC_DEBUG_SYS, "+\n");

	switch (hiusbc->support_dr_mode) {
	case HIUSBC_DR_MODE_HOST:
	case HIUSBC_DR_MODE_DEVICE:
		hiusbc_exit_current_mode(hiusbc);
		break;
	case HIUSBC_DR_MODE_BOTH:
		hiusbc_drd_exit(hiusbc);
		break;
	default:
		break;
	}
}

void hiusbc_set_mode(struct hiusbc *hiusbc,
			enum hiusbc_dr_mode desired_mode)
{
	enum hiusbc_dr_mode current_mode = hiusbc->current_dr_mode;
	unsigned long flags;

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "current_mode = %d, desired_mode = %d\n",
		current_mode, desired_mode);

	if (desired_mode == current_mode ||
		desired_mode == HIUSBC_DR_MODE_BOTH)
		return;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->desired_dr_mode = desired_mode;
	spin_unlock_irqrestore(&hiusbc->lock, flags);

	hiusbc_exit_current_mode(hiusbc);

	hiusbc_enter_desired_mode(hiusbc);

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "-\n");
}

int hiusbc_switch_mode(u32 usbc_id, enum hiusbc_dr_mode mode)
{
	if (usbc_id < 0 || usbc_id >= USBC_NUM) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR,
		"[uid=%d] hiusbc switch mode failed! usbc:%u err.\n",
		__kuid_val(current_uid()), usbc_id);
		return DRV_ERROR_PARA_ERROR;
	}
	if (usbc_id == 0) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR,
		"[uid=%d] hiusbc switch mode failed! usbc0 not support switch mode.\n",
		__kuid_val(current_uid()));
		return DRV_ERROR_NOT_SUPPORT;
	}
	if (mode != HIUSBC_DR_MODE_DEVICE && mode != HIUSBC_DR_MODE_HOST) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR,
			"[uid=%d] hiusbc switch mode failed! usbc:%u mode:%d err.\n",
			__kuid_val(current_uid()), usbc_id, mode);
		return DRV_ERROR_PARA_ERROR;
	}
	if (g_hiusbcs[usbc_id] == NULL) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR,
			"[uid=%d] hiusbc switch mode failed! usbc:%u not exist.\n",
			__kuid_val(current_uid()), usbc_id);
		return DRV_ERROR_NOT_EXIST;
	}

	mutex_lock(&g_mode_mutex);
	hiusbc_set_mode(g_hiusbcs[usbc_id], mode);
	if (mode != g_hiusbcs[usbc_id]->current_dr_mode) {
		mutex_unlock(&g_mode_mutex);
		hiusbc_dbg(HIUSBC_DEBUG_ERR,
			"[uid=%d] hiusbc switch mode failed! usbc:%u mode:%d.\n",
			__kuid_val(current_uid()), usbc_id, mode);
		return DRV_ERROR_INNER_ERR;
	}
	mutex_unlock(&g_mode_mutex);
	hiusbc_dbg(HIUSBC_DEBUG_INFO,
		"[uid=%d] hiusbc switch mode success! usbc:%u mode:%d.\n",
		__kuid_val(current_uid()), usbc_id, mode);
	return 0;
}
EXPORT_SYMBOL(hiusbc_switch_mode);

static void hiusbc_get_properties(struct hiusbc *hiusbc)
{
	struct device *dev = hiusbc->dev;
	u32 mode;
	hiusbc_dbg(HIUSBC_DEBUG_SYS, "+\n");

	if (device_property_read_u32(dev, "mode", &mode))
		mode = HIUSBC_DR_MODE_HOST;
	hiusbc->support_dr_mode = mode;
	hiusbc_dbg(HIUSBC_DEBUG_INFO, "get mode = %d\n", mode);

	if (device_property_read_bool(dev, "linux,sysdev_is_parent"))
		hiusbc->sysdev = dev->parent;
	else
		hiusbc->sysdev = dev;

	hiusbc->max_speed = usb_get_maximum_speed(dev);
	hiusbc_dbg(HIUSBC_DEBUG_SYS, "max_speed = %d\n", hiusbc->max_speed);
	if (hiusbc->max_speed == USB_SPEED_UNKNOWN)
		hiusbc->max_speed = USB_SPEED_SUPER;

	hiusbc->hird_suspend = 12;

	hiusbc->usb2_dev_lpm_capable = 0;
	hiusbc->usb3_dev_lpm_capable = 0;
	hiusbc->usb3_dev_lpm_u1_accept = 1;
	hiusbc->usb3_dev_lpm_u2_accept = 1;
	hiusbc->usb3_dev_lpm_u1_initiate = 1;
	hiusbc->usb3_dev_lpm_u2_initiate = 1;
	hiusbc->usb3_dev_lpm_ux_exit = 0;
	hiusbc->dis_gadget_u2_lpm_bulk_always_idle = 0;

	hiusbc->usb2_host_lpm_capable = 0;
	hiusbc->usb3_host_lpm_capable = 0;
	hiusbc->sg_supported = 0;
}

static int hiusbc_init_regs(struct hiusbc *hiusbc, struct resource *res)
{
	void __iomem *regs = NULL;
	int ret = 0;

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "+\n");
	res->start += HIUSBC_DEVICE_MODE_REG_BASE;

	regs = devm_ioremap_resource(hiusbc->dev, res);
	if (IS_ERR(regs)) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "hiusbc ioremap fail.\n");
		ret = PTR_ERR(regs);
		goto exit;
	}

	hiusbc->regs = regs;
	hiusbc->regs_size = resource_size(res);

	hiusbc->host_regs = regs + HOST_BASE_ADDR;
	hiusbc->dev_regs = regs + DEV_BASE_ADDR;
	hiusbc->com_regs = regs + COM_BASE_ADDR;
	hiusbc->hub_regs = regs + HUB_BASE_ADDR;

	hiusbc->lmi_regs = regs + LMI_BASE_ADDR;

	hiusbc->u2_piu_regs = regs + U2PM_BASE_ADDR;
	hiusbc->u3_link_regs = regs + U3LINK_BASE_ADDR;

	hiusbc_dbg(HIUSBC_DEBUG_SYS,
		"base address = 0x%pK, dev_regs = 0x%pK, u3_link_regs = 0x%pK\n",
		regs, hiusbc->dev_regs,
		hiusbc->u3_link_regs);

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "-\n");

	return ret;

exit:
	res->start -= HIUSBC_DEVICE_MODE_REG_BASE;
	return ret;
}

static void hiusbc_clear_regs(struct hiusbc *hiusbc, struct resource *res)
{
	hiusbc_dbg(HIUSBC_DEBUG_SYS, "+\n");

	res->start -= HIUSBC_DEVICE_MODE_REG_BASE;
	devm_iounmap(hiusbc->dev, hiusbc->regs);
	hiusbc->regs = NULL;
	hiusbc->regs_size = 0;

	hiusbc->host_regs = NULL;
	hiusbc->dev_regs = NULL;
	hiusbc->com_regs = NULL;
	hiusbc->hub_regs = NULL;
	hiusbc->lmi_regs = NULL;
	hiusbc->u2_piu_regs = NULL;
	hiusbc->u3_link_regs = NULL;
	devm_iounmap(hiusbc->dev, hiusbc->io_subctrl_regs);
	hiusbc->io_subctrl_regs = NULL;
	devm_iounmap(hiusbc->dev, hiusbc->pcs_regs);
	hiusbc->pcs_regs = NULL;
}

static void hiusbc_dfx_start(struct hiusbc *hiusbc)
{
	u32 reg;

	hiusbc->ltssm_dump = 1;

	reg = hiusbc_readl(hiusbc->u3_link_regs,
		CSR_DFX_STATE_CFG_OFFSET);
	reg |= CSR_DFX_STATE_WR_EN_MASK;
	hiusbc_writel(reg, hiusbc->u3_link_regs,
		CSR_DFX_STATE_CFG_OFFSET);
}

#ifdef CONFIG_USB_HIUSBCV100_BUGFIX_PATCH
static void hisi_vendor_usb_ignore_babble_er(void *data, struct xhci_td *td, struct xhci_ring *ep_ring, int *status)
{
	td = list_first_entry(&ep_ring->td_list, struct xhci_td, td_list);

	/* Is this an Int/Isoc EP in a LS/FS device under an external HS hub? */
	if (td->urb->dev->tt && td->urb->dev->tt->hub->parent &&
		(usb_endpoint_xfer_int(&td->urb->ep->desc) || usb_endpoint_xfer_isoc(&td->urb->ep->desc))) {
		hiusbc_dbg(HIUSBC_DEBUG_INFO, "HIUSBC: Ignore Babble error on endpoint\n");
		*status = 0;
	} else {
		*status = -EOVERFLOW;
	}
}

static void hisi_vendor_usb_get_u3_port_status(void *data, u32 link_state, bool *wake_resume)
{
	if (link_state == XDEV_U3) {
		*wake_resume = false;
	}
}

static unsigned int xhci_microframes_to_exponent(struct usb_device *udev, struct usb_host_endpoint *ep,
	unsigned int desc_interval, unsigned int min_exponent, unsigned int max_exponent)
{
	unsigned int interval;

	interval = fls(desc_interval) - 1;
	interval = clamp_val(interval, min_exponent, max_exponent);
	if ((1 << interval) != desc_interval) {
		dev_dbg(&udev->dev, "ep %#x - rounding interval to %d microframes, ep desc says %d microframes\n",
			ep->desc.bEndpointAddress, 1 << interval, desc_interval);
	}

	return interval;
}

static void hisi_vendor_usb_parse_frame_interval(void *data, struct usb_device *udev, struct usb_host_endpoint *ep,
	u32 *ret)
{
	if ((udev->tt && udev->tt->hub->parent) && (usb_endpoint_xfer_int(&ep->desc))) {
		*ret = xhci_microframes_to_exponent(udev, ep, ep->desc.bInterval * 8, 7, 10); // 8 7 10 refer to (16ms)
	} else {
		*ret = xhci_microframes_to_exponent(udev, ep, ep->desc.bInterval * 8, 3, 10); // 8 3 10 refer to default value
	}
	return;
}

static void hisi_vendor_usb_endpoint_init(void *data, struct usb_device *udev, struct usb_host_endpoint *ep,
	u32 *max_esit_payload, u32 *max_packet)
{
	if (!udev->tt || !udev->tt->hub->parent) {
		return;
	}

	if (usb_endpoint_xfer_isoc(&ep->desc)) {
		if (usb_endpoint_dir_out(&ep->desc)) {
			hiusbc_dbg(HIUSBC_DEBUG_INFO, "WARN: ep %u(isoc out), from mesit=%u, maxp=%u\n",
				usb_endpoint_num(&ep->desc), *max_esit_payload, *max_packet);
			*max_esit_payload = ((*max_esit_payload + USB_HEADSET_LOW_RESOLUTION_MAX_ESIT - 1) /
				USB_HEADSET_LOW_RESOLUTION_MAX_ESIT) * USB_HEADSET_LOW_RESOLUTION_MAX_ESIT; // when <191, set 192
		} else {
			hiusbc_dbg(HIUSBC_DEBUG_INFO, "WARN: ep %u(isoc in), from mesit=%u, maxp=%u\n",
				usb_endpoint_num(&ep->desc), *max_esit_payload, *max_packet);
			if (*max_esit_payload <= USB_HEADSET_LOW_RESOLUTION_MAX_ESIT) { // when <=192, set 192+175=367
				*max_esit_payload = USB_HEADSET_LOW_RESOLUTION_MAX_ESIT + USB_HEADSET_LOW_RESOLUTION_PAD;
			} else if (*max_esit_payload <= USB_HEADSET_MEDIUM_RESOLUTION_MAX_ESIT) { // when <=384, set 384+171=555
				*max_esit_payload = USB_HEADSET_MEDIUM_RESOLUTION_MAX_ESIT + USB_HEADSET_MEDIUM_RESOLUTION_PAD;
			} else if (*max_esit_payload <= USB_HEADSET_HIGH_RESOLUTION_MAX_ESIT) { // when <=576, set 576+167=743
				*max_esit_payload = USB_HEADSET_HIGH_RESOLUTION_MAX_ESIT + USB_HEADSET_HIGH_RESOLUTION_PAD;
			}
		}
		*max_packet = *max_esit_payload;
		hiusbc_dbg(HIUSBC_DEBUG_INFO, "to mesit=%u, maxp=%u!\n", *max_esit_payload, *max_packet);
	}

	if ((usb_endpoint_xfer_int(&ep->desc)) && (usb_endpoint_dir_in(&ep->desc))) {
		hiusbc_dbg(HIUSBC_DEBUG_INFO, "WARN: ep %u(int in), from mesit=%u, maxp=%u\n",
			usb_endpoint_num(&ep->desc), *max_esit_payload, *max_packet);
		if ((udev->speed == USB_SPEED_FULL) && (*max_esit_payload <= USB_FS_INT_IN_MAX_ESIT_PAYLOAD)) { // when <=350
			*max_esit_payload = USB_FS_INT_IN_MAX_ESIT_PAYLOAD; // set max_eist = 350Byte, max_pkt_size = 64Byte
			*max_packet = USB_FS_INT_IN_MAX_PACKET;
		}
		if ((udev->speed == USB_SPEED_LOW) && (*max_esit_payload <= USB_LS_INT_IN_MAX_ESIT_PAYLOAD)) { // when <=9
			*max_esit_payload = USB_LS_INT_IN_MAX_ESIT_PAYLOAD; // set max_esit = 9Byte, max_pkt_size = 8Byte
			*max_packet = USB_LS_INT_IN_MAX_PACKET;
		}
		hiusbc_dbg(HIUSBC_DEBUG_INFO, "to mesit=%u, maxp=%u!\n", *max_esit_payload, *max_packet);
	}
}

static int vendor_usbxhci_hooks_register(void)
{
	int ret = 0;

	ret = register_trace_vendor_usb_ignore_babble_err(&hisi_vendor_usb_ignore_babble_er, NULL);
	if (ret != 0) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "usb register_trace_vendor_usb_ignore_babble_err fail.\n");
		return ret;
	}

	ret = register_trace_vendor_usb_get_u3_port_status(&hisi_vendor_usb_get_u3_port_status, NULL);
	if (ret != 0) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "usb register_trace_vendor_usb_get_u3_port_status fail.\n");
		goto ignore_babble_err;
	}

	ret = register_trace_vendor_usb_parse_frame_interval(&hisi_vendor_usb_parse_frame_interval, NULL);
	if (ret != 0) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "usb register_trace_vendor_usb_parse_frame_interval fail.\n");
		goto get_u3_port_status;
	}

	ret = register_trace_vendor_usb_endpoint_init(&hisi_vendor_usb_endpoint_init, NULL);
	if (ret != 0) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "usb register_trace_vendor_usb_endpoint_init fail.\n");
		goto parse_frame_interval;
	}
	return 0;

parse_frame_interval:
	unregister_trace_vendor_usb_parse_frame_interval(&hisi_vendor_usb_parse_frame_interval, NULL);
get_u3_port_status:
	unregister_trace_vendor_usb_get_u3_port_status(&hisi_vendor_usb_get_u3_port_status, NULL);
ignore_babble_err:
	unregister_trace_vendor_usb_ignore_babble_err(&hisi_vendor_usb_ignore_babble_er, NULL);
	return ret;
}

static void vendor_usbxhci_hooks_unregister(void)
{
	unregister_trace_vendor_usb_ignore_babble_err(&hisi_vendor_usb_ignore_babble_er, NULL);
	unregister_trace_vendor_usb_get_u3_port_status(&hisi_vendor_usb_get_u3_port_status, NULL);
	unregister_trace_vendor_usb_parse_frame_interval(&hisi_vendor_usb_parse_frame_interval, NULL);
	unregister_trace_vendor_usb_endpoint_init(&hisi_vendor_usb_endpoint_init, NULL);
}
#endif

STATIC int hiusbc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res = NULL;
	struct hiusbc *hiusbc = NULL;
	int ret;

	hiusbc_dbg(HIUSBC_DEBUG_INFO, "+\n");
	hiusbc = devm_kzalloc(dev, sizeof(*hiusbc), GFP_KERNEL);
	if (!hiusbc) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "hiusbc alloc failed!\n");
		return -ENOMEM;
	}

	hiusbc->dev = dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "Can't get memory resource of hiusbc.\n");
		ret = -ENODEV;
		goto exit;
	}

	ret = hiusbc_init_regs(hiusbc, res);
	if (ret) {
		dev_err(dev, "Failed to initial registers of hiusbc.\n");
		goto exit;
	}

	hiusbc_get_properties(hiusbc);

	platform_set_drvdata(pdev, hiusbc);

	spin_lock_init(&hiusbc->lock);

	ret = hiusbc_asic_init(hiusbc);
	if (ret) {
		dev_err(dev, "Failed to initial asic regs.\n");
		goto asic_err;
	}

	pm_runtime_set_active(dev);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_set_autosuspend_delay(dev, HIUSBC_AUTOSUSPEND_DELAY);
	pm_runtime_enable(dev);
	ret = pm_runtime_get_sync(dev);
	if (ret < 0)
		goto pm_err;

	pm_runtime_forbid(dev);

	hiusbc_dfx_start(hiusbc);

	ret = hiusbc_init_dr_mode(hiusbc);
	if (ret) {
		dev_err(dev, "Failed to initial dr mode of hiusbc.\n");
		goto dr_mode_err;
	}

	ret = hiusbc_gpio_config(hiusbc);
	if (ret) {
		dev_err(dev, "Failed to initial gpio config.\n");
		goto gpio_cfg_err;
	}

	hiusbc_debugfs_init(hiusbc);

	pm_runtime_put(dev);

	g_hiusbcs[hiusbc->usb_id] = hiusbc;
	hiusbc_dbg(HIUSBC_DEBUG_SYS, "-\n");
	return 0;

gpio_cfg_err:
	hiusbc_exit_dr_mode(hiusbc);

dr_mode_err:
	pm_runtime_allow(dev);

pm_err:
	pm_runtime_put_sync(dev);
	pm_runtime_disable(dev);
	hiusbc_asic_exit(hiusbc);

asic_err:
	hiusbc_clear_regs(hiusbc, res);
	platform_set_drvdata(pdev, NULL);

exit:
	devm_kfree(dev, hiusbc);
	hiusbc_dbg(HIUSBC_DEBUG_ERR, "-: ret = %d\n", ret);
	return ret;
}

STATIC void hiusbc_remove(struct platform_device *pdev)
{
	struct hiusbc *hiusbc = platform_get_drvdata(pdev);
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	struct device *dev = &pdev->dev;

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "+\n");

	pm_runtime_get_sync(dev);

	hiusbc_debugfs_exit(hiusbc);

	hiusbc_exit_dr_mode(hiusbc);

	pm_runtime_allow(dev);

	pm_runtime_put_sync(dev);
	pm_runtime_disable(dev);
	hiusbc_gpio_free(hiusbc);
	hiusbc_asic_exit(hiusbc);
	hiusbc_clear_regs(hiusbc, res);
	platform_set_drvdata(pdev, NULL);
	g_hiusbcs[hiusbc->usb_id] = NULL;
	devm_kfree(dev, hiusbc);
}
#ifdef HIUSBC_PM
static int hiusbc_common_suspend(struct hiusbc *hiusbc)
{
	unsigned long flags;

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "+\n");

	spin_lock_irqsave(&hiusbc->lock, flags);
	if (hiusbc->current_dr_mode == HIUSBC_DR_MODE_DEVICE) {
		if (!hiusbc->gadget_driver)
			goto exit;

		hiusbc_run_stop(hiusbc, 0);
		hiusbc_gadget_disconnect(hiusbc);
		hiusbc_stop(hiusbc);
	}

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "-\n");

exit:
	spin_unlock_irqrestore(&hiusbc->lock, flags);

	return 0;
}

static int hiusbc_common_resume(struct hiusbc *hiusbc)
{
	unsigned long flags;
	int ret = 0;

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "+\n");

	spin_lock_irqsave(&hiusbc->lock, flags);

	if (hiusbc->current_dr_mode == HIUSBC_DR_MODE_DEVICE) {
		if (!hiusbc->gadget_driver)
			goto exit;

		ret = hiusbc_init_phy(hiusbc);
		if (ret) {
			dev_err(hiusbc->dev,
				"failed to init phy during %s.\n", __func__);
			goto exit;
		}

		ret = hiusbc_run_stop(hiusbc, 1);
		if (ret) {
			hiusbc_run_stop(hiusbc, 0);
			goto exit;
		}
	}

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "-\n");

exit:
	spin_unlock_irqrestore(&hiusbc->lock, flags);
	return ret;
}

static int hiusbc_runtime_suspend(struct device *dev)
{
	struct hiusbc *hiusbc = dev_get_drvdata(dev);
	int ret;

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "+\n");

	if (hiusbc->current_dr_mode == HIUSBC_DR_MODE_DEVICE &&
		hiusbc->connected)
		return -EBUSY;

	ret = hiusbc_common_suspend(hiusbc);
	if (ret)
		return ret;

	device_init_wakeup(dev, true);

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "-\n");

	return ret;
}

static int hiusbc_runtime_resume(struct device *dev)
{
	struct hiusbc *hiusbc = dev_get_drvdata(dev);
	int ret;

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "+\n");

	device_init_wakeup(dev, false);

	ret = hiusbc_common_resume(hiusbc);
	if (ret)
		return ret;

	pm_runtime_mark_last_busy(dev);
	pm_runtime_put(dev);

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "-\n");

	return ret;
}

static int hiusbc_runtime_idle(struct device *dev)
{
	struct hiusbc *hiusbc = dev_get_drvdata(dev);

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "+\n");

	if (hiusbc->current_dr_mode == HIUSBC_DR_MODE_DEVICE &&
		hiusbc->connected)
		return -EBUSY;

	pm_runtime_mark_last_busy(dev);
	pm_runtime_autosuspend(dev);

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "-\n");

	return 0;
}

static int hiusbc_suspend(struct device *dev)
{
	struct hiusbc *hiusbc = dev_get_drvdata(dev);
	int ret;

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "+\n");

	ret = hiusbc_common_suspend(hiusbc);
	if (ret)
		return ret;

	pinctrl_pm_select_sleep_state(dev);

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "-\n");

	return ret;
}

static int hiusbc_resume(struct device *dev)
{
	struct hiusbc *hiusbc = dev_get_drvdata(dev);
	int ret;

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "+\n");

	pinctrl_pm_select_default_state(dev);

	ret = hiusbc_common_resume(hiusbc);
	if (ret)
		return ret;

	pm_runtime_disable(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "-\n");

	return 0;
}
#endif

static int hiusbc_device_mode_suspend(struct hiusbc *hiusbc)
{
	u8 epnum;
	unsigned long flags;

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "+\n");

	for (epnum = 0; epnum < HIUSBC_EP_NUMS; epnum++) {
		hiusbc->eps[epnum]->xfer_state = HIUSBC_XFER_END;
	}

	spin_lock_irqsave(&hiusbc->lock, flags);
	if (hiusbc->current_dr_mode == HIUSBC_DR_MODE_DEVICE) {
		if (!hiusbc->gadget_driver)
			goto exit;

		hiusbc_run_stop(hiusbc, 0);
		hiusbc_gadget_disconnect(hiusbc);
		hiusbc_stop(hiusbc);
	}

exit:
	spin_unlock_irqrestore(&hiusbc->lock, flags);
	hiusbc_dbg(HIUSBC_DEBUG_SYS, "-\n");

	return 0;
}

static int hiusbc_device_mode_resume(struct hiusbc *hiusbc)
{
	int ret = 0;

	hiusbc_dbg(HIUSBC_DEBUG_SYS, "+\n");

	ret = hiusbc_soft_rst_switch_clk(hiusbc);
	if (ret) {
		dev_err(hiusbc->dev, "failed to rst after switch clk! ret = %d\n", ret);
		goto exit;
	}

	ret =hiusbc_run_stop(hiusbc, 1);
	if (ret) {
		hiusbc_run_stop(hiusbc, 0);
		dev_err(hiusbc->dev, "Failed to start usb ctrl! ret = %d\n", ret);
		goto exit;
	}

exit:
	hiusbc_dbg(HIUSBC_DEBUG_SYS, "-\n");
	return ret;
}

static int usb_suspend(struct device *dev)
{
	int ret = 0;
	struct hiusbc *hiusbc;

	mutex_lock(&g_mode_mutex);
	hiusbc = dev_get_drvdata(dev);

	ret = hiusbc_gpio_lpmset(hiusbc, HIUSBC_SUSPEND);
	if (ret) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "gpio setup failed!\n");
		goto exit;
	}

	if (hiusbc->current_dr_mode == HIUSBC_DR_MODE_DEVICE) {
		ret = hiusbc_device_mode_suspend(hiusbc);
		if (ret) {
			hiusbc_dbg(HIUSBC_DEBUG_ERR, " hiusbc suspend failed!\n");
			goto exit;
		}
		mdelay(200);
	}

	hiusbc_save_qos_cfg();

exit:
	mutex_unlock(&g_mode_mutex);

	return ret;
}

static int usb_resume(struct device *dev)
{
	struct hiusbc *hiusbc;
	int ret = 0;

	mutex_lock(&g_mode_mutex);
	hiusbc_restore_qos_cfg();
	hiusbc = dev_get_drvdata(dev);

	ret = hiusbc_set_qos_cfg(0, &g_qos_record);
	if (ret) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "set qos cfg failed!\n");
		goto exit;
	}

	ret = hiusbc_gpio_lpmset(hiusbc, HIUSBC_RESUME);
	if (ret) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "gpio setup gpio failed!\n");
		goto exit;
	}
	ret = hiusbc_soc_init(hiusbc);
	if (ret) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, " hiusbc soc init failed!\n");
		goto exit;
	}
	hiusbc_constraints_init(hiusbc);
	hiusbc_init_phy(hiusbc);

	if (hiusbc->current_dr_mode == HIUSBC_DR_MODE_HOST) {
		hiusbc_host_config_init(hiusbc);
	} else {
		ret = hiusbc_device_mode_resume(hiusbc);
		if (ret) {
			hiusbc_dbg(HIUSBC_DEBUG_ERR, " hiusbc resume failed!\n");
			goto exit;
		}
	}

exit:
	mutex_unlock(&g_mode_mutex);
	return ret;
}

static const struct dev_pm_ops hiusbc_dev_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(usb_suspend, usb_resume)
#ifdef HIUSBC_PM
	SET_SYSTEM_SLEEP_PM_OPS(hiusbc_suspend, hiusbc_resume)
	SET_RUNTIME_PM_OPS(hiusbc_runtime_suspend, hiusbc_runtime_resume,
			hiusbc_runtime_idle)
#endif
};

static const struct of_device_id of_hiusbc_match[] = {
	{ .compatible = "hisilicon,hiusbc" },
	{ .compatible = "hisi,hiusbc" },
	{},
};

static struct platform_driver hiusbc_driver = {
	.probe		= hiusbc_probe,
	.remove		= hiusbc_remove,
	.driver		= {
		.name	= "hiusbc",
		.pm	= &hiusbc_dev_pm_ops,
		.of_match_table	= of_hiusbc_match,
	},
};

static int __init hiusbc_module_init(void)
{
	int ret;
	mutex_init(&g_mode_mutex);
	ret = platform_driver_register(&hiusbc_driver);

	if (ret != 0) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "usb driver_register fail.\n");
		return ret;
	}

#ifdef CONFIG_USB_HIUSBCV100_BUGFIX_PATCH
	// resgister usb hook
	ret = vendor_usbxhci_hooks_register();
	if (ret != 0) {
		platform_driver_unregister(&hiusbc_driver);
		return ret;
	}
#endif

	return 0;
}
module_init(hiusbc_module_init);

static void __exit hiusbc_mode_exit(void)
{
	mutex_destroy(&g_mode_mutex);
#ifdef CONFIG_USB_HIUSBCV100_BUGFIX_PATCH
	vendor_usbxhci_hooks_unregister();
#endif
	platform_driver_unregister(&hiusbc_driver);
}
module_exit(hiusbc_mode_exit);

MODULE_DESCRIPTION("Driver for the Hisilicon USB Controller");
MODULE_ALIAS("platform:hiusbc");
MODULE_LICENSE("GPL v2");
