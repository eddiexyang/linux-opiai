/*
 * hiusbc_debugfs.c -- Debugfs for Hisilicon USB Controller.
 *
 * Copyright (c) 2019 Hisilicon Technologies CO., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

//#include <securec.h>
#include "include/securec.h"
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/usb/ch9.h>
#include "hiusbc_core.h"
#include "hiusbc_debugfs_common.h"
#include "hiusbc_gadget.h"
#include "hiusbc_setup.h"
#include "hiusbc_debugfs.h"

#define MAX_BUF_SIZE	32
#define MAX_LOG_OPS_NUM 14
#define HIUSBC_LOG_LEVEL_HIGHEST	0xffffffff
#define HIUSBC_LOG_LEVEL_LOWEST		0
#ifdef HIUSBC_UT
u32 hiusbc_log_level = 0;
#else
u32 hiusbc_log_level = HIUSBC_DEBUG_ERR;
#endif

#ifdef CFG_BUILD_DEBUG
#define SYSFS_FILE_PERMISSION 0600
#else
#define SYSFS_FILE_PERMISSION 0400
#endif
extern struct mutex g_mode_mutex;
struct log_ops {
	char on[MAX_BUF_SIZE];
	char off[MAX_BUF_SIZE];
	unsigned int log_level ;
};

struct log_ops log_ops_arr[MAX_LOG_OPS_NUM] = {
	{"log_on", "log_off", 0},
	{"err_on", "err_off", HIUSBC_DEBUG_ERR},
	{"sys_on", "sys_off", HIUSBC_DEBUG_SYS},
	{"ep_on", "ep_off", HIUSBC_DEBUG_EP},
	{"cmd_on", "cmd_off", HIUSBC_DEBUG_CMD},
	{"event_on", "event_off", HIUSBC_DEBUG_EVENT},
	{"xfer_on", "xfer_off", HIUSBC_DEBUG_XFER},
	{"ctrl_on", "ctrl_off", HIUSBC_DEBUG_CTRL},
	{"isoc_on", "isoc_off", HIUSBC_DEBUG_ISOC},
	{"bulk_on", "bulk_off", HIUSBC_DEBUG_BULK},
	{"int_on", "int_off", HIUSBC_DEBUG_INT},
	{"temp_on", "temp_off", HIUSBC_DEBUG_TEMP},
	{"evt_ring_on", "evt_ring_off", HIUSBC_DEBUG_EVT_RING},
	{"xfer_ring_on", "xfer_ring_off", HIUSBC_DEBUG_XFER_RING}
};

struct hiusbc_debugfs_item {
	u32 val;
	const char *string;
};

struct hiusbc_debugfs_cb {
	const char *string;
	void (*cb)(struct hiusbc *hiusbc);
};

#define dump_register_dev(nm ...)			\
{						\
	.name	= #nm,				\
	.offset	= DEV_BASE_ADDR +	\
		nm##_OFFSET - HIUSBC_DEVICE_MODE_REG_BASE,	\
}

#define dump_register_com(nm ...)			\
{						\
	.name	= #nm,				\
	.offset	= COM_BASE_ADDR +	\
		nm##_OFFSET - HIUSBC_DEVICE_MODE_REG_BASE,	\
}

static const struct debugfs_reg32 hiusbc_regs[] = {
	dump_register_dev(DEV_USB3_U1EL),
	dump_register_dev(DEV_USB3_U2EL),
	dump_register_dev(DEV_CFG),
	dump_register_dev(DEV_CTL),
	dump_register_dev(DEV_RST),
	dump_register_dev(DEV_STATUS),
	dump_register_dev(DEV_EVENT_EN),
	dump_register_dev(DEV_EVENT),
	dump_register_dev(DEV_U2_PORTSC),
	dump_register_dev(DEV_U3_PORTSC),
	dump_register_dev(DEV_U2PORT_TEST_MODE),

	dump_register_com(DRD_MODE),
	dump_register_com(CSR_RST_N),
	dump_register_com(U3_CONTINUE_MODE),
};

STATIC int hiusbc_log_level_show(struct seq_file *s, void *reserved)
{
	struct hiusbc *hiusbc = s->private;
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	seq_printf(s, "current log_level 0x%x\n", hiusbc_log_level);
	spin_unlock_irqrestore(&hiusbc->lock, flags);

	return 0;
}

STATIC int hiusbc_log_level_open(struct inode *inode,
				struct file *file)
{
	return single_open(file, hiusbc_log_level_show, inode->i_private);
}

STATIC ssize_t hiusbc_log_level_write(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos)
{
	char buf[MAX_BUF_SIZE] = {0};
	int i;

	if (copy_from_user(buf, ubuf, min_t(size_t, sizeof(buf) - 1, count))) {
		hiusbc_fs_err("set log level fail!\n");
		return -EFAULT;
	}

	for (i = 0; i < MAX_LOG_OPS_NUM; i++) {
		if ((i == 0) && !strncmp(buf, log_ops_arr[i].on, strlen(log_ops_arr[i].on))) {
			hiusbc_log_level = HIUSBC_LOG_LEVEL_HIGHEST;
			hiusbc_fs_info("set log level success!\n");
			return count;
		} else if ((i == 0) && !strncmp(buf, log_ops_arr[i].off, strlen(log_ops_arr[i].off))) {
			hiusbc_log_level = HIUSBC_LOG_LEVEL_LOWEST;
			hiusbc_fs_info("set log level success!\n");
			return count;
		} else if (!strncmp(buf, log_ops_arr[i].on, strlen(log_ops_arr[i].on))) {
			hiusbc_log_level |= log_ops_arr[i].log_level;
			hiusbc_fs_info("set log level success!\n");
			return count;
		} else if (!strncmp(buf, log_ops_arr[i].off, strlen(log_ops_arr[i].off))) {
			hiusbc_log_level &= ~log_ops_arr[i].log_level;
			hiusbc_fs_info("set log level success!\n");
			return count;
		}
	}
	hiusbc_fs_err("set log level fail!\n");
	return -EINVAL;
}

static const struct file_operations hiusbc_log_level_fops = {
	.open			= hiusbc_log_level_open,
	.write			= hiusbc_log_level_write,
	.read			= seq_read,
	.llseek			= seq_lseek,
	.release		= single_release,
};

STATIC int hiusbc_mode_show(struct seq_file *s, void *unused)
{
	struct hiusbc *hiusbc = s->private;

	switch (hiusbc->current_dr_mode) {
	case HIUSBC_DR_MODE_HOST:
		seq_puts(s, "host\n");
		break;
	case HIUSBC_DR_MODE_DEVICE:
		seq_puts(s, "device\n");
		break;
	default:
		seq_printf(s, "UNKNOWN\n");
	}

	return 0;
}

STATIC int hiusbc_mode_open(struct inode *inode, struct file *file)
{
	return single_open(file, hiusbc_mode_show, inode->i_private);
}

#ifdef CFG_BUILD_DEBUG
STATIC ssize_t hiusbc_mode_write(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct hiusbc *hiusbc = s->private;
	enum hiusbc_dr_mode mode = HIUSBC_DR_MODE_UNKNOWN;
	char buf[MAX_BUF_SIZE] = {0};

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count))) {
		hiusbc_fs_err("set usbc mode fail!\n");
		return -EFAULT;
	}

	if (!strncmp(buf, "host", strlen("host")))
		mode = HIUSBC_DR_MODE_HOST;

	if (!strncmp(buf, "device", strlen("device")))
		mode = HIUSBC_DR_MODE_DEVICE;

	mutex_lock(&g_mode_mutex);
	hiusbc_set_mode(hiusbc, mode);
	mutex_unlock(&g_mode_mutex);
	hiusbc_fs_info("set usbc mode success!\n");
	return count;
}
#endif

static const struct file_operations hiusbc_mode_fops = {
	.open			= hiusbc_mode_open,
#ifdef CFG_BUILD_DEBUG
	.write			= hiusbc_mode_write,
#endif
	.read			= seq_read,
	.llseek			= seq_lseek,
	.release		= single_release,
};

static const struct hiusbc_debugfs_item u2_link_state_string[] = {
	{.val = HIUSBC_LINK_STATE_L3, .string = "L3"},
	{.val = HIUSBC_LINK_STATE_RESET, .string = "RESET"},
	{.val = HIUSBC_LINK_STATE_L1, .string = "L1"},
	{.val = HIUSBC_LINK_STATE_L2, .string = "L2"},
	{.val = HIUSBC_LINK_STATE_L0, .string = "L0"},
	{.val = HIUSBC_LINK_STATE_TEST, .string = "TEST"},
	{.val = HIUSBC_LINK_STATE_L1_RESUME, .string = "L1 RESUME"},
	{.val = HIUSBC_LINK_STATE_L2_RESUME, .string = "L2 RESUME"}
};

static const struct hiusbc_debugfs_item u3_link_state_string[] = {
	{.val = HIUSBC_LINK_STATE_U0, .string = "U0"},
	{.val = HIUSBC_LINK_STATE_U1, .string = "U1"},
	{.val = HIUSBC_LINK_STATE_U2, .string = "U2"},
	{.val = HIUSBC_LINK_STATE_U3, .string = "U3"},
	{.val = HIUSBC_LINK_STATE_ESS_DIS, .string = "eSS.Disabled"},
	{.val = HIUSBC_LINK_STATE_RX_DET, .string = "RX.Detect"},
	{.val = HIUSBC_LINK_STATE_ESS_INAC, .string = "eSS.Inactive"},
	{.val = HIUSBC_LINK_STATE_POLLING, .string = "Polling"},
	{.val = HIUSBC_LINK_STATE_RECOVERY, .string = "Recovery"},
	{.val = HIUSBC_LINK_STATE_HOT_RESET, .string = "Hot Reset"},
	{.val = HIUSBC_LINK_STATE_COMPLIANCE, .string = "Compliance"},
	{.val = HIUSBC_LINK_STATE_LOOPBACK, .string = "Loopback"}
};

STATIC const char *hiusbc_link_string(enum usb_device_speed link_speed,
			enum hiusbc_link_state link_state)
{
	u32 i, size = 0;
	const struct hiusbc_debugfs_item *string = NULL;

	if (link_speed == USB_SPEED_UNKNOWN)
		return "UNKNOWN link speed\n";

	if (link_speed <= USB_SPEED_HIGH) {
		size = ARRAY_SIZE(u2_link_state_string);
		string = u2_link_state_string;
	}

	if (link_speed >= USB_SPEED_SUPER) {
		size = ARRAY_SIZE(u3_link_state_string);
		string = u3_link_state_string;
	}

	for (i = 0; i < size; i++) {
		if (link_state == string[i].val)
			return string[i].string;
	}
	return "UNKNOWN link state\n";
}

STATIC int hiusbc_link_state_show(struct seq_file *s, void *unused)
{
	struct hiusbc *hiusbc = s->private;
	unsigned long flags;
	enum usb_device_speed speed;
	enum hiusbc_link_state state;

	spin_lock_irqsave(&hiusbc->lock, flags);
	speed = hiusbc_get_link_speed(hiusbc);
	state = hiusbc_get_link_state(hiusbc, speed);
	spin_unlock_irqrestore(&hiusbc->lock, flags);

	seq_printf(s, "%s\n", hiusbc_link_string(speed, state));

	return 0;
}

STATIC int hiusbc_link_state_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, hiusbc_link_state_show, inode->i_private);
}

#ifdef CFG_BUILD_DEBUG
STATIC ssize_t hiusbc_link_state_write(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct hiusbc *hiusbc = s->private;
	unsigned long flags;
	enum hiusbc_link_state state;
	enum usb_device_speed speed = USB_SPEED_SUPER;
	char buf[MAX_BUF_SIZE] = {0};

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count))) {
		hiusbc_fs_err("set link state fail!\n");
		return -EFAULT;
	}

	if (!strncmp(buf, "eSS.Disabled", strlen("eSS.Disabled"))) {
		state = HIUSBC_LINK_STATE_ESS_DIS;
	} else if (!strncmp(buf, "eSS.Inactive", strlen("eSS.Inactive"))) {
		state = HIUSBC_LINK_STATE_ESS_INAC;
	} else if (!strncmp(buf, "Recovery", strlen("Recovery"))) {
		state = HIUSBC_LINK_STATE_RECOVERY;
	} else if (!strncmp(buf, "L0", strlen("L0"))) {
		speed = USB_SPEED_HIGH;
		state = HIUSBC_LINK_STATE_L0;
	} else {
		hiusbc_fs_err("set link state fail!\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc_set_link_state(hiusbc, speed, state);
	spin_unlock_irqrestore(&hiusbc->lock, flags);
	hiusbc_fs_info("set link state success!\n");
	return count;
}
#endif

static const struct file_operations hiusbc_link_state_fops = {
	.open			= hiusbc_link_state_open,
#ifdef CFG_BUILD_DEBUG
	.write			= hiusbc_link_state_write,
#endif
	.read			= seq_read,
	.llseek			= seq_lseek,
	.release		= single_release,
};

STATIC int hiusbc_current_speed_show(struct seq_file *s,
			void *reserved)
{
	struct hiusbc *hiusbc = s->private;
	enum usb_device_speed current_speed;
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	current_speed = hiusbc_get_link_speed(hiusbc);
	seq_printf(s, "current current_speed %s\n",
			usb_speed_string(current_speed));
	spin_unlock_irqrestore(&hiusbc->lock, flags);

	return 0;
}

STATIC int hiusbc_current_speed_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, hiusbc_current_speed_show, inode->i_private);
}

#ifdef CFG_BUILD_DEBUG
STATIC ssize_t hiusbc_current_speed_write(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct hiusbc *hiusbc = s->private;
	unsigned long flags;
	u32 current_speed;
	char buf[MAX_BUF_SIZE] = {0};

	if (copy_from_user(buf, ubuf, min_t(size_t, sizeof(buf) - 1, count))) {
		hiusbc_fs_err("set current speed fail!\n");
		return -EFAULT;
	}
	if (!strncmp(buf, "super+", strlen("super+"))) {
		current_speed = USB_SPEED_SUPER_PLUS;
	} else if (!strncmp(buf, "super", strlen("super"))) {
		current_speed = USB_SPEED_SUPER;
	} else if (!strncmp(buf, "high", strlen("high"))) {
		current_speed = USB_SPEED_HIGH;
	} else if (!strncmp(buf, "full", strlen("full"))) {
		current_speed = USB_SPEED_FULL;
	} else {
		hiusbc_fs_err("set current speed fail!\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc_set_speed(hiusbc, current_speed);
	spin_unlock_irqrestore(&hiusbc->lock, flags);
	hiusbc_fs_info("set current speed success!\n");
	return count;
}
#endif

static const struct file_operations hiusbc_current_speed_fops = {
	.open			= hiusbc_current_speed_open,
#ifdef CFG_BUILD_DEBUG
	.write			= hiusbc_current_speed_write,
#endif
	.read			= seq_read,
	.llseek			= seq_lseek,
	.release		= single_release,
};

#ifdef CFG_BUILD_DEBUG
static const struct hiusbc_debugfs_item testmode[] = {
	{.val = 0, .string = "no test\n"},
	{.val = USB_TEST_J, .string = "test_j\n"},
	{.val = USB_TEST_K, .string = "test_k\n"},
	{.val = USB_TEST_SE0_NAK, .string = "test_se0_nak\n"},
	{.val = USB_TEST_PACKET, .string = "test_packet\n"},
	{.val = USB_TEST_FORCE_ENABLE, .string = "test_force_enable\n"}
};

STATIC int hiusbc_testmode_show(struct seq_file *s, void *unused)
{
	struct hiusbc *hiusbc = s->private;
	unsigned long flags;
	u32 reg, mode, i;

	spin_lock_irqsave(&hiusbc->lock, flags);
	reg = hiusbc_readl(hiusbc->dev_regs,
		DEV_U2PORT_TEST_MODE_OFFSET);
	spin_unlock_irqrestore(&hiusbc->lock, flags);

	mode = hiusbc_get_u2_test_mode(reg);

	for (i = 0; i < ARRAY_SIZE(testmode); i++) {
		if (mode == testmode[i].val) {
			seq_printf(s, testmode[i].string);
			break;
		}
	}

	if (i == ARRAY_SIZE(testmode))
		seq_printf(s, "UNKNOWN %d\n", mode);

	return 0;
}

STATIC int hiusbc_testmode_open(struct inode *inode,
			struct file *file)
{
	return single_open(file, hiusbc_testmode_show, inode->i_private);
}

STATIC ssize_t hiusbc_testmode_write(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct hiusbc *hiusbc = s->private;
	unsigned long flags;
	u8 mode = 0;
	char buf[MAX_BUF_SIZE] = {0};
	u32 reg, i;
	u32 remote_wakeup_backup;

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count))) {
		hiusbc_fs_err("set test mode fail!\n");
		return -EFAULT;
	}

	for (i = 0; i < ARRAY_SIZE(testmode); i++) {
		if (!strncmp(buf, testmode[i].string,
				strlen(testmode[i].string))) {
			mode = testmode[i].val;
			break;
		}
	}

	spin_lock_irqsave(&hiusbc->lock, flags);
	reg = hiusbc_readl(hiusbc->dev_regs,
			DEV_CFG_OFFSET);
	reg &= ~LPM_ENABLE_MASK;
	hiusbc_writel(reg, hiusbc->dev_regs,
			DEV_CFG_OFFSET);

	remote_wakeup_backup = hiusbc->remote_wakeup;
	hiusbc->remote_wakeup = true;
	if (hiusbc_wakeup(hiusbc))
		hiusbc_dbg(HIUSBC_DEBUG_ERR,
				"failed to set link state to L0\n");
	hiusbc_req_set_test_mode(hiusbc, mode);
	hiusbc->remote_wakeup = remote_wakeup_backup;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
	hiusbc_fs_info("set test mode success!\n");
	return count;
}

static const struct file_operations hiusbc_testmode_fops = {
	.open			= hiusbc_testmode_open,
	.write			= hiusbc_testmode_write,
	.read			= seq_read,
	.llseek			= seq_lseek,
	.release		= single_release,
};

STATIC int hiusbc_maximum_speed_show(struct seq_file *s,
				void *reserved)
{
	struct hiusbc *hiusbc = s->private;
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	seq_printf(s, "current maximum_speed %s\n"
		      "Usage:\n"
		      " write \"full\" to force USB full-speed\n"
		      " write \"high\" to force USB high-speed\n",
		      usb_speed_string(hiusbc->max_speed));
	spin_unlock_irqrestore(&hiusbc->lock, flags);

	return 0;
}

STATIC int hiusbc_maximum_speed_open(struct inode *inode,
			struct file *file)
{
	return single_open(file, hiusbc_maximum_speed_show, inode->i_private);
}

STATIC ssize_t hiusbc_maximum_speed_write(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct hiusbc *hiusbc = s->private;
	unsigned long flags;
	u32 maximum_speed;
	char buf[MAX_BUF_SIZE] = {0};

	if (copy_from_user(buf, ubuf, min_t(size_t, sizeof(buf) - 1, count))) {
		hiusbc_fs_err("set maximum speed fail!\n");
		return -EFAULT;
	}

	if (!strncmp(buf, "super+", strlen("super+"))) {
		maximum_speed = USB_SPEED_SUPER_PLUS;
	} else if (!strncmp(buf, "super", strlen("super"))) {
		maximum_speed = USB_SPEED_SUPER;
	} else if (!strncmp(buf, "high", strlen("high"))) {
		maximum_speed = USB_SPEED_HIGH;
	} else if (!strncmp(buf, "full", strlen("full"))) {
		maximum_speed = USB_SPEED_FULL;
	} else if (!strncmp(buf, "clear", strlen("clear"))) {
		maximum_speed = USB_SPEED_UNKNOWN;
	} else {
		hiusbc_fs_err("set maximum speed fail!\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->max_speed = maximum_speed;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
	hiusbc_fs_info("set maximum speed success!\n");
	return count;
}

static const struct file_operations hiusbc_maximum_speed_fops = {
	.open			= hiusbc_maximum_speed_open,
	.write			= hiusbc_maximum_speed_write,
	.read			= seq_read,
	.llseek			= seq_lseek,
	.release		= single_release,
};

STATIC int hiusbc_cp_test_show(struct seq_file *s, void *reserved)
{
	struct hiusbc *hiusbc = s->private;
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	seq_printf(s, "[cptest mode: %u]\n", hiusbc->cp_test);
	spin_unlock_irqrestore(&hiusbc->lock, flags);

	return 0;
}

STATIC int hiusbc_cp_test_open(struct inode *inode, struct file *file)
{
	return single_open(file, hiusbc_cp_test_show, inode->i_private);
}

STATIC ssize_t hiusbc_cp_test_write(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file		*s = file->private_data;
	struct hiusbc		*hiusbc = s->private;
	unsigned long		flags;
	char			buf[MAX_BUF_SIZE] = {0};

	if (copy_from_user(buf, ubuf, min_t(size_t, sizeof(buf) - 1, count))) {
		hiusbc_fs_err("set cp fail!\n");
		return -EFAULT;
	}

	if (!strncmp(buf, "cp0", strlen("cp0"))) {
		spin_lock_irqsave(&hiusbc->lock, flags);
		hiusbc_cptest_set_to_cp0(hiusbc);
		spin_unlock_irqrestore(&hiusbc->lock, flags);
		hiusbc_fs_info("set cp success!\n");
		return count;
	}

	if (!strncmp(buf, "next_pattern", strlen("next_pattern"))) {
		spin_lock_irqsave(&hiusbc->lock, flags);
		hiusbc_cptest_next_pattern(hiusbc);
		spin_unlock_irqrestore(&hiusbc->lock, flags);
		hiusbc_fs_info("set cp success!\n");
		return count;
	}

	hiusbc_fs_err("set cp fail!\n");
	return -EINVAL;
}

static const struct file_operations hiusbc_cp_test_fops = {
	.open			= hiusbc_cp_test_open,
	.write			= hiusbc_cp_test_write,
	.read			= seq_read,
	.llseek			= seq_lseek,
	.release		= single_release,
};

STATIC int hiusbc_lpm_show(struct seq_file *s, void *reserved)
{
	struct hiusbc *hiusbc = s->private;
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	seq_printf(s, "u2 host lpm is %s\n"
		      "u2 device lpm is %s\n"
		      "u3 host lpm is %s\n"
		      "u3 device lpm is %s\n"
		      "u3 device lpm u1 accept is %s\n"
		      "u3 device lpm u2 accept is %s\n"
		      "u3 device lpm u1 initiate is %s\n"
		      "u3 device lpm u2 initiate is %s\n",
		      hiusbc->usb2_host_lpm_capable ? "on" : "off",
		      hiusbc->usb2_dev_lpm_capable ? "on" : "off",
		      hiusbc->usb3_host_lpm_capable ? "on" : "off",
		      hiusbc->usb3_dev_lpm_capable ? "on" : "off",
		      hiusbc->usb3_dev_lpm_u1_accept ? "on" : "off",
		      hiusbc->usb3_dev_lpm_u2_accept ? "on" : "off",
		      hiusbc->usb3_dev_lpm_u1_initiate ? "on" : "off",
		      hiusbc->usb3_dev_lpm_u2_initiate ? "on" : "off");
	spin_unlock_irqrestore(&hiusbc->lock, flags);

	return 0;
}

STATIC int hiusbc_lpm_open(struct inode *inode, struct file *file)
{
	return single_open(file, hiusbc_lpm_show, inode->i_private);
}

STATIC void usb2_host_lpm_on(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->usb2_host_lpm_capable = 1;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void usb2_host_lpm_off(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->usb2_host_lpm_capable = 0;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void usb2_device_lpm_on(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->usb2_dev_lpm_capable = 1;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void usb2_device_lpm_off(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->usb2_dev_lpm_capable = 0;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void usb3_host_lpm_on(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->usb3_host_lpm_capable = 1;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void usb3_host_lpm_off(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->usb3_host_lpm_capable = 0;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void usb3_device_lpm_on(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->usb3_dev_lpm_capable = 1;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void usb3_device_lpm_off(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->usb3_dev_lpm_capable = 0;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void usb3_u1_accept_on(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->usb3_dev_lpm_u1_accept = 1;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void usb3_u1_accept_off(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->usb3_dev_lpm_u1_accept = 0;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void usb3_u2_accept_on(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->usb3_dev_lpm_u2_accept = 1;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void usb3_u2_accept_off(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->usb3_dev_lpm_u2_accept = 0;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void usb3_u1_init_on(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->usb3_dev_lpm_u1_initiate = 1;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void usb3_u1_init_off(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->usb3_dev_lpm_u1_initiate = 0;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void usb3_u2_init_on(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->usb3_dev_lpm_u2_initiate = 1;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void usb3_u2_init_off(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->usb3_dev_lpm_u2_initiate = 0;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void usb3_ux_exit_on(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->usb3_dev_lpm_ux_exit = 1;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void usb3_ux_exit_off(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->usb3_dev_lpm_ux_exit = 0;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

static const struct hiusbc_debugfs_cb lpm_option[] = {
	{.string = "usb2_host_on", .cb = usb2_host_lpm_on},
	{.string = "usb2_host_off", .cb = usb2_host_lpm_off},
	{.string = "usb2_device_on", .cb = usb2_device_lpm_on},
	{.string = "usb2_device_off", .cb = usb2_device_lpm_off},
	{.string = "usb3_host_on", .cb = usb3_host_lpm_on},
	{.string = "usb3_host_off", .cb = usb3_host_lpm_off},
	{.string = "usb3_device_on", .cb = usb3_device_lpm_on},
	{.string = "usb3_device_off", .cb = usb3_device_lpm_off},
	{.string = "u1_accept_on", .cb = usb3_u1_accept_on},
	{.string = "u1_accept_off", .cb = usb3_u1_accept_off},
	{.string = "u2_accept_on", .cb = usb3_u2_accept_on},
	{.string = "u2_accept_off", .cb = usb3_u2_accept_off},
	{.string = "u1_init_on", .cb = usb3_u1_init_on},
	{.string = "u1_init_off", .cb = usb3_u1_init_off},
	{.string = "u2_init_on", .cb = usb3_u2_init_on},
	{.string = "u2_init_off", .cb = usb3_u2_init_off},
	{.string = "ux_exit_on", .cb = usb3_ux_exit_on},
	{.string = "ux_exit_off", .cb = usb3_ux_exit_off}
};

STATIC ssize_t hiusbc_lpm_write(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct hiusbc *hiusbc = s->private;
	char buf[MAX_BUF_SIZE] = {0};
	u32 i;

	if (copy_from_user(buf, ubuf, min_t(size_t, sizeof(buf) - 1, count))) {
		hiusbc_fs_err("set lpm fail!\n");
		return -EFAULT;
	}

	for (i = 0; i < ARRAY_SIZE(lpm_option); i++) {
		if (!strncmp(buf, lpm_option[i].string,
				strlen(lpm_option[i].string))) {
			lpm_option[i].cb(hiusbc);
			hiusbc_fs_info("set lpm success!\n");
			return count;
		}
	}
	hiusbc_fs_err("set lpm fail!\n");
	return -EINVAL;
}

static const struct file_operations hiusbc_lpm_fops = {
	.open			= hiusbc_lpm_open,
	.write			= hiusbc_lpm_write,
	.read			= seq_read,
	.llseek			= seq_lseek,
	.release		= single_release,
};

STATIC int hiusbc_fpga_debug_show(struct seq_file *s,
			void *reserved)
{
	struct hiusbc *hiusbc = s->private;
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	seq_printf(s, "db_on_received_event is %s\n",
			hiusbc->db_on_received_event ? "on" : "off");
	spin_unlock_irqrestore(&hiusbc->lock, flags);

	return 0;
}

STATIC int hiusbc_fpga_debug_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, hiusbc_fpga_debug_show, inode->i_private);
}

static const struct hiusbc_debugfs_item eps_info[] = {
	{.val = 0, .string = "ep0"},
	{.val = 1, .string = "ep1"},
	{.val = 2, .string = "ep2"},
	{.val = 3, .string = "ep3"},
	{.val = 4, .string = "ep4"},
	{.val = 5, .string = "ep5"},
	{.val = 6, .string = "ep6"},
	{.val = 7, .string = "ep7"},
	{.val = 8, .string = "ep8"},
	{.val = 9, .string = "ep9"}
};

static const struct hiusbc_debugfs_item knock_eps_info[] = {
	{.val = 2, .string = "knock_ep2"},
	{.val = 3, .string = "knock_ep3"},
	{.val = 4, .string = "knock_ep4"},
	{.val = 5, .string = "knock_ep5"},
	{.val = 6, .string = "knock_ep6"},
	{.val = 7, .string = "knock_ep7"},
	{.val = 8, .string = "knock_ep8"},
	{.val = 9, .string = "knock_ep9"},
	{.val = 10, .string = "knock_ep10"},
	{.val = 11, .string = "knock_ep11"},
	{.val = 12, .string = "knock_ep12"},
	{.val = 13, .string = "knock_ep13"},
	{.val = 14, .string = "knock_ep14"},
	{.val = 15, .string = "knock_ep15"},
	{.val = 16, .string = "knock_ep16"}
};

static const struct hiusbc_debugfs_item force_maxp[] = {
	{.val = 4, .string = "quarter_maxp"},
	{.val = 2, .string = "half_maxp"},
	{.val = 1, .string = "normal_maxp"}
};

STATIC void usb3_more_db(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->db_on_received_event = 1;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void usb3_less_db(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->db_on_received_event = 0;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void usb3_trpu_reset_on(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->trpu_reset_switch = 1;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void usb3_trpu_reset_off(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->trpu_reset_switch = 0;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void usb3_stop_switch_on(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->stop_before_drd_switch = 1;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void usb3_stop_switch_off(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->stop_before_drd_switch = 0;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void usb3_reset_switch_on(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->reset_before_drd_switch = 1;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void usb3_reset_switch_off(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->reset_before_drd_switch = 0;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void usb3_dev_event(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc_show_dev_event_counter(hiusbc);
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void disconnect_in_suspend_on(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->disconnect_in_suspend = 1;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void disconnect_in_suspend_off(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->disconnect_in_suspend = 0;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void sg_on(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->sg_supported = 1;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void sg_off(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->sg_supported = 0;
	spin_unlock_irqrestore(&hiusbc->lock, flags);
}
#endif

STATIC void ltssm_dump_on(struct hiusbc *hiusbc)
{
	unsigned long flags;
	u32 reg;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->ltssm_dump = 1;

	reg = hiusbc_readl(hiusbc->u3_link_regs,
		CSR_DFX_STATE_CFG_OFFSET);
	reg |= CSR_DFX_STATE_WR_EN_MASK;
	hiusbc_writel(reg, hiusbc->u3_link_regs,
		CSR_DFX_STATE_CFG_OFFSET);

	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void ltssm_dump_off(struct hiusbc *hiusbc)
{
	unsigned long flags;
	u32 reg;

	spin_lock_irqsave(&hiusbc->lock, flags);
	hiusbc->ltssm_dump = 0;

	reg = hiusbc_readl(hiusbc->u3_link_regs,
		CSR_DFX_STATE_CFG_OFFSET);
	reg &= ~CSR_DFX_STATE_WR_EN_MASK;
	hiusbc_writel(reg, hiusbc->u3_link_regs,
		CSR_DFX_STATE_CFG_OFFSET);

	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void ltssm_dump_clear(struct hiusbc *hiusbc)
{
	unsigned long flags;
	u32 reg_dfx;

	spin_lock_irqsave(&hiusbc->lock, flags);
	reg_dfx = hiusbc_readl(hiusbc->u3_link_regs,
		CSR_DFX_STATE_CFG_OFFSET);
	reg_dfx |= CSR_DFX_STATE_CLR_MASK;
	hiusbc_writel(reg_dfx, hiusbc->u3_link_regs,
		CSR_DFX_STATE_CFG_OFFSET);

	udelay(10);

	reg_dfx &= ~CSR_DFX_STATE_CLR_MASK;
	hiusbc_writel(reg_dfx, hiusbc->u3_link_regs,
		CSR_DFX_STATE_CFG_OFFSET);

	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

const char *ltssm_state_string[78] = {
	"EDD_INIT", "EDD_RST", "EDD_PWC", "EDD_MAIN",
	"EDD_DEFAULT", "EDD_ERROR", "EI_PWC", "EI_QUIET",
	"EI_DIS_DET", "RXD_INIT", "RXD_PWC0", "RXD_RST",
	"RXD_WR0", "RXD_PWC1", "RXD_WR1", "RXD_PWC2",
	"RXD_ACTIVE", "RXD_QUIET", "POLL_PWC", "POLL_LFPS",
	"POLL_LPFSP", "POLL_PM", "POLL_RATEC", "POLL_PC",
	"POLL_RXEQ", "POLL_ACTIVE", "POLL_CONFIG", "POLL_IDLE",
	"RECOV_ACTIVE", "RECOV_CONFIG", "RECOV_IDLE", "HR_ACTIVE",
	"HR_EXIT", "CP_RATE", "CP_NORMAL", "LPBK_M",
	"LPBK_S", "LPBK_EXIT_IV_STAGE1", "LPBK_EXIT_IV_STAGE2", "LPBK_EXIT_PV",
	"U0", "U1_ENTRY_PWC", "U1_MAIN", "U1_WAIT_PV",
	"U1_EXIT_PWC0", "U1_EXIT_PWC1", "U1_EXIT_IV_STAGE1", "U1_EXIT_IV_STAGE2",
	"U1_EXIT_PV", "U2_ENTRY_PWC", "U2_MAIN", "U2_RXD",
	"U2_WAIT_PV", "U2_EXIT_PWC0", "U2_EXIT_PV", "U2_EXIT_PWC1",
	"U2_EXIT_IV_STAGE1", "U2_EXIT_IV_STAGE2", "U3_ENTRY_PWC", "U3_MAIN",
	"U3_RXD", "U3_WAIT_PV", "U3_EXIT_PWC0", "U3_EXIT_PV",
	"U3_EXIT_PWC1", "U3_EXIT_IV_STAGE1", "U3_EXIT_IV_STAGE2", "U3_FAIL_PWC",
	"RECOV_RXD_TMP", "RXD_WAIT_PHY_READY", "RXD_PWR_CHG", "RXD_RATE_CHG",
	"LPBK_PWR_CHG", "LPBK_RATE_CHG", "FAKE_U0", "EDD_RXD_P3",
	"EDD_RXD_P2", "POLL_LPFS_RATEC"
};

STATIC int hiusbc_ltssm_dump_show(struct seq_file* s,
	void *reserved)
{
	struct hiusbc *hiusbc = s->private;
	u32 ltssm_dump[33] = {0};
	u64 ltssm_tmp = 0;
	u8 state[155] = {0};
	int index = 0;
	u8 count = 0;
	u8 print_index = 0;

	seq_printf(s, "start dump ltssm:\n");
	for (index = 0; index <= 32; index++)
		ltssm_dump[index] = hiusbc_readl(hiusbc->u3_link_regs,
			CSR_STATE_DFX_REG0_OFFSET + index * 4);

	for (index = 32; index > 0; index -= 4) {
		seq_printf(s, "0x%x, 0x%x, 0x%x, 0x%x, \n",
			ltssm_dump[index],
			ltssm_dump[index - 1],
			ltssm_dump[index - 2],
			ltssm_dump[index - 3]);

		if (index == 4)
			seq_printf(s, "0x%x\n",
				ltssm_dump[index - 4]);
	}

	for (index = 32; index >= 0; index-=2) {
		if (count == 0)
			ltssm_tmp = ((u64)ltssm_dump[index] << (32 - count)) |
						(ltssm_dump[index - 1] >> count);
		else if (count == 16)
			ltssm_tmp = ((u64)ltssm_dump[index + 1] << (64 - count)) |
						((u64)ltssm_dump[index] << (32 - count));
		else
			ltssm_tmp = ((u64)ltssm_dump[index + 1] << (64 - count)) |
						((u64)ltssm_dump[index] << (32 - count)) |
						(ltssm_dump[index - 1] >> count);

		for (print_index = 1; print_index <= 9; print_index++)
			state[count * 9 + print_index - 1] = (u8)(ltssm_tmp >> (64 - (print_index * 7))) & 0x7f;
		count++;
	}

	seq_printf(s, "show ltssm:\n");
	for (index = 0; index < 150; index += 3)
		seq_printf(s, "%u.%s << %u.%s << %u.%s <<\n",
			state[index], ltssm_state_string[state[index]],
			state[index + 1], ltssm_state_string[state[index + 1]],
			state[index + 2], ltssm_state_string[state[index + 2]]);

	return 0;
}

static const struct hiusbc_debugfs_cb ltssm_dump_option[] = {
	{.string = "dump_on", .cb = ltssm_dump_on},
	{.string = "dump_off", .cb = ltssm_dump_off},
	{.string = "dump_clr", .cb = ltssm_dump_clear},
};

STATIC ssize_t hiusbc_ltssm_dump_write (struct file *file,
	const char __user * ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *seq = file->private_data;
	struct hiusbc *hiusbc = seq->private;
	char buf[MAX_BUF_SIZE] = {0};
	u32 i;

	if (copy_from_user(buf, ubuf, min_t(size_t, sizeof(buf) - 1, count))) {
		hiusbc_fs_err("set ltssm dump error\n");
		return -EFAULT;
	}

	for (i = 0; i < ARRAY_SIZE(ltssm_dump_option); i++) {
		if (!strncmp(buf, ltssm_dump_option[i].string,
			strlen(ltssm_dump_option[i].string))) {
			ltssm_dump_option[i].cb(hiusbc);
			hiusbc_fs_info("set ltssm dump success\n");
			return count;
		}
	}
	hiusbc_fs_err("set ltssm dump error\n");
	return -EINVAL;
}

STATIC int hiusbc_ltssm_dump_open(struct inode *inode,
	struct file *file)
{
	return single_open(file, hiusbc_ltssm_dump_show, inode->i_private);
}

static const struct file_operations hiusbc_ltssm_dump_fops = {
	.open			= hiusbc_ltssm_dump_open,
	.write			= hiusbc_ltssm_dump_write,
	.read			= seq_read,
	.llseek			= seq_lseek,
	.release		= single_release,
};

#ifdef CFG_BUILD_DEBUG
static const struct hiusbc_debugfs_cb fpga_debug_option[] = {
	{.string = "more_db", .cb = usb3_more_db},
	{.string = "less_db", .cb = usb3_less_db},
	{.string = "trpu_reset_on", .cb = usb3_trpu_reset_on},
	{.string = "trpu_reset_off", .cb = usb3_trpu_reset_off},
	{.string = "stop_switch_on", .cb = usb3_stop_switch_on},
	{.string = "stop_switch_off", .cb = usb3_stop_switch_off},
	{.string = "reset_switch_on", .cb = usb3_reset_switch_on},
	{.string = "reset_switch_off", .cb = usb3_reset_switch_off},
	{.string = "dev_event", .cb = usb3_dev_event},
	{.string = "disc_in_sus_on", .cb = disconnect_in_suspend_on},
	{.string = "disc_in_sus_off", .cb = disconnect_in_suspend_off},
	{.string = "sg_on", .cb = sg_on},
	{.string = "sg_off", .cb = sg_off},
};

STATIC ssize_t hiusbc_fpga_debug_write(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct hiusbc *hiusbc = s->private;
	u32 i;
	char buf[MAX_BUF_SIZE] = {0};

	if (copy_from_user(buf, ubuf, min_t(size_t, sizeof(buf) - 1, count))) {
		hiusbc_fs_err("set debug option fail!\n");
		return -EFAULT;
	}

	for (i = 0; i < ARRAY_SIZE(fpga_debug_option); i++) {
		if (!strncmp(buf, fpga_debug_option[i].string,
			strlen(fpga_debug_option[i].string))) {
			fpga_debug_option[i].cb(hiusbc);
			hiusbc_fs_info("set debug option success!\n");
			return count;
		}
	}

	for (i = 0; i < ARRAY_SIZE(eps_info); i++) {
		if (!strncmp(buf, eps_info[i].string,
			strlen(eps_info[i].string))) {
			hiusbc_show_xfer_event_counter(
					hiusbc->eps[eps_info[i].val]);
			hiusbc_fs_info("set debug option success!\n");
			return count;
		}
	}

	for (i = 0; i < ARRAY_SIZE(knock_eps_info); i++) {
		if (!strncmp(buf, knock_eps_info[i].string,
			strlen(knock_eps_info[i].string))) {
			hiusbc_debug_knock_ep(
					hiusbc->eps[knock_eps_info[i].val]);
			hiusbc_fs_info("set debug option success!\n");
			return count;
		}
	}
	hiusbc_fs_err("set debug option fail!\n");
	return -EINVAL;
}

static const struct file_operations hiusbc_fpga_debug_fops = {
	.open			= hiusbc_fpga_debug_open,
	.write			= hiusbc_fpga_debug_write,
	.read			= seq_read,
	.llseek			= seq_lseek,
	.release		= single_release,
};

STATIC int hiusbc_phy_show(struct seq_file *s,
			void *reserved)
{
	struct hiusbc *hiusbc = s->private;
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);
	seq_printf(s, "TBD: Print top ctrl regs\n");
	spin_unlock_irqrestore(&hiusbc->lock, flags);

	return 0;
}

STATIC int hiusbc_phy_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, hiusbc_phy_show, inode->i_private);
}
STATIC void check_phy_clk_lock_rt(struct hiusbc *hiusbc)
{
	unsigned long flags;
	u32 reg;

	spin_lock_irqsave(&hiusbc->lock, flags);

	// read result.
	reg = hiusbc_readl(hiusbc->hub_regs,
		CUR_LOC_STATUS_OFFSET);
	// show u2 phy clk result.
	if (reg & CUR_CLK_U2PHY_LOC_MASK) {
		hiusbc_dbg(HIUSBC_DEBUG_TEMP, "u2 phy clk NG!\n");
	} else {
		hiusbc_dbg(HIUSBC_DEBUG_TEMP, "u2 phy clk OK!\n");
	}

	// show u3 phy clk result.
	if (reg & CUR_CLK_PIPE_LOC_MASK) {
		hiusbc_dbg(HIUSBC_DEBUG_TEMP, "u3 phy clk NG!\n");
	} else {
		hiusbc_dbg(HIUSBC_DEBUG_TEMP, "u3 phy clk OK!\n");
	}

	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void check_phy_clk_freq(struct hiusbc *hiusbc)
{
	unsigned long flags;
	u32 reg;
	u8 u2_done = 0;
	u8 u3_done = 0;
	u32 timeout = 1000;
	u64 result;

	spin_lock_irqsave(&hiusbc->lock, flags);

	// enable phy clk freq exam.
	reg = hiusbc_readl(hiusbc->hub_regs,
		FREQ_EXAM_EN_OFFSET);
	reg &= ~CLK_U2PHY_EXAM_EN_MASK;
	reg &= ~CLK_PIPE_EXAM_EN_MASK;
	hiusbc_writel(reg, hiusbc->hub_regs,
		FREQ_EXAM_EN_OFFSET);

	reg = hiusbc_readl(hiusbc->hub_regs,
		FREQ_EXAM_EN_OFFSET);
	reg |= CLK_U2PHY_EXAM_EN_MASK;
	reg |= CLK_PIPE_EXAM_EN_MASK;
	hiusbc_writel(reg, hiusbc->hub_regs,
		FREQ_EXAM_EN_OFFSET);

	// read result.
	do {
		reg = hiusbc_readl(hiusbc->hub_regs,
			FREQ_EXAM_DONE_OFFSET);
		if (reg & CLK_U2PHY_FREQ_EXAM_DONE_MASK)
			u2_done = 1;
		if (reg & CLK_PIPE_FREQ_EXAM_DONE_MASK)
			u3_done = 1;
		udelay(1);
	} while (!(u2_done && u3_done) && --timeout);

	// show phy clk freq result.
	reg = hiusbc_readl(hiusbc->hub_regs,
		FREQ_EXAM_COUNTER_OFFSET);
	if (u2_done) {
		result  = ((u64)(reg & CLK_U2PHY_FREQ_EXAM_COUNTER_MASK) *
			232000) / 0x400;
		hiusbc_dbg(HIUSBC_DEBUG_TEMP, "u2 phy clk freq = %lld or %lld KHz.\n", result, (result * 77 / 232));
	} else {
		hiusbc_dbg(HIUSBC_DEBUG_TEMP, "u2 phy clk freq timeout!!!\n");
	}

	if (u3_done) {
		result  = ((u64)((reg & CLK_PIPE_FREQ_EXAM_COUNTER_MASK) >>
			CLK_PIPE_FREQ_EXAM_COUNTER_SHIFT) * 232000)/ 0x400;
		hiusbc_dbg(HIUSBC_DEBUG_TEMP, "u3 phy clk freq = %lld or %lld KHz/\n", result, (result * 77 / 232));
	} else {
		hiusbc_dbg(HIUSBC_DEBUG_TEMP, "u3 phy clk freq timeout!!!\n");
	}

	// disable phy clk freq exam.
	reg = hiusbc_readl(hiusbc->hub_regs,
		FREQ_EXAM_EN_OFFSET);
	reg &= ~CLK_U2PHY_EXAM_EN_MASK;
	reg &= ~CLK_PIPE_EXAM_EN_MASK;
	hiusbc_writel(reg, hiusbc->hub_regs,
		FREQ_EXAM_EN_OFFSET);

	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void set_phy_sel_mode(struct hiusbc *hiusbc, u32 value)
{
	u32 reg;

	reg = hiusbc_readl(hiusbc->com_regs,
		PHY_SEL_MODE_OFFSET);
	reg &= ~PHY_SEL_MODE_MASK;
	reg |= hiusbc_phy_set(value);
	hiusbc_writel(reg, hiusbc->com_regs,
		PHY_SEL_MODE_OFFSET);

	reg = hiusbc_readl(hiusbc->com_regs,
		U3_CONTINUE_MODE_OFFSET);
	if (value == HIUSBC_PHY_SET_U2_ONLY) {
		reg |= USBC_U2_EN_MASK;
		reg &= ~USBC_U3_EN_MASK;
	} else if (value == HIUSBC_PHY_SET_U3_ONLY) {
		reg &= ~USBC_U2_EN_MASK;
		reg |= USBC_U3_EN_MASK;
	} else {
		reg |= USBC_U2_EN_MASK;
		reg |= USBC_U3_EN_MASK;
	}
	if (value == HIUSBC_PHY_SET_U3_ONLY) {
		// if u3 link fail, not switch to u2.
		reg |= U3_CONTINUE_MODE_MASK;
	} else {
		// if u3 link fail, switch to u2.
		reg &= ~U3_CONTINUE_MODE_MASK;
	}
	hiusbc_writel(reg, hiusbc->com_regs,
	U3_CONTINUE_MODE_OFFSET);
}

STATIC void set_phy_u2only(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);

	/* only turn on U2 phy. */
	set_phy_sel_mode(hiusbc, HIUSBC_PHY_SET_U2_ONLY);

	hiusbc_dbg(HIUSBC_DEBUG_TEMP, "DONE!\n");

	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void set_phy_u3only(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);

	/* only turn on U3 phy. */
	set_phy_sel_mode(hiusbc, HIUSBC_PHY_SET_U3_ONLY);

	hiusbc_dbg(HIUSBC_DEBUG_TEMP, "DONE!\n");

	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void set_phy_u2u3(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);

	/* only turn on U2 and U3 phy. */
	set_phy_sel_mode(hiusbc, HIUSBC_PHY_SET_BOTH);

	hiusbc_dbg(HIUSBC_DEBUG_TEMP, "DONE!\n");

	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void set_vbus_sel_mode(struct hiusbc *hiusbc, u32 value)
{
	u32 reg;

	reg = hiusbc_readl(hiusbc->com_regs,
		VBUS_SEL_MODE_OFFSET);
	reg &= ~VBUS_SEL_MODE_MASK;
	reg |= hiusbc_vbus_sel_mode(value);
	hiusbc_writel(reg, hiusbc->com_regs,
		VBUS_SEL_MODE_OFFSET);
}

STATIC void set_u2_vbus_sel(struct hiusbc *hiusbc, u32 value)
{
	u32 reg;

	reg = hiusbc_readl(hiusbc->com_regs,
		U2_VBUS_SEL_OFFSET);
	reg &= ~U2_VBUS_SEL_MASK;
	reg |= hiusbc_u2_vbus_sel(value);
	hiusbc_writel(reg, hiusbc->com_regs,
		U2_VBUS_SEL_OFFSET);
}

STATIC void set_u3_vbus_sel(struct hiusbc *hiusbc, u32 value)
{
	u32 reg;

	reg = hiusbc_readl(hiusbc->com_regs,
		U3_VBUS_SEL_OFFSET);
	reg &= ~U3_VBUS_SEL_MASK;
	reg |= hiusbc_u3_vbus_sel(value);
	hiusbc_writel(reg, hiusbc->com_regs,
		U3_VBUS_SEL_OFFSET);
}

STATIC void set_u2_vbus_force_value(struct hiusbc *hiusbc, u32 value)
{
	u32 reg;

	reg = hiusbc_readl(hiusbc->com_regs,
		U2_VBUS_FCE_EN_OFFSET);
	if (value) {
		reg |= U2_VBUS_FCE_EN_MASK;
	} else {
		reg &= ~U2_VBUS_FCE_EN_MASK;
	}
	hiusbc_writel(reg, hiusbc->com_regs,
		U2_VBUS_FCE_EN_OFFSET);
}

STATIC void set_u3_vbus_force_value(struct hiusbc *hiusbc, u32 value)
{
	u32 reg;

	reg = hiusbc_readl(hiusbc->com_regs,
		U3_VBUS_FCE_EN_OFFSET);
	if (value) {
		reg |= U3_VBUS_FCE_EN_MASK;
	} else {
		reg &= ~U3_VBUS_FCE_EN_MASK;
	}
	hiusbc_writel(reg, hiusbc->com_regs,
		U3_VBUS_FCE_EN_OFFSET);
}

STATIC void set_vbus_from_soc(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);

	set_vbus_sel_mode(hiusbc, HIUSBC_VBUS_U2);
	set_u2_vbus_sel(hiusbc, HIUSBC_U2_VBUS_FROM_SOC);

	hiusbc_dbg(HIUSBC_DEBUG_TEMP, "DONE!\n");

	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void set_vbus_from_u2phy(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);

	set_vbus_sel_mode(hiusbc, HIUSBC_VBUS_U2);
	set_u2_vbus_sel(hiusbc, HIUSBC_U2_VBUS_FROM_ULPI);

	hiusbc_dbg(HIUSBC_DEBUG_TEMP, "DONE!\n");

	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void set_vbus_from_u2force_on(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);

	set_vbus_sel_mode(hiusbc, HIUSBC_VBUS_U2);
	set_u2_vbus_sel(hiusbc, HIUSBC_U2_VBUS_FORCE);
	set_u2_vbus_force_value(hiusbc, 1);

	hiusbc_dbg(HIUSBC_DEBUG_TEMP, "DONE!\n");

	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void set_vbus_from_u2force_off(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);

	set_vbus_sel_mode(hiusbc, HIUSBC_VBUS_U2);
	set_u2_vbus_sel(hiusbc, HIUSBC_U2_VBUS_FORCE);
	set_u2_vbus_force_value(hiusbc, 0);

	hiusbc_dbg(HIUSBC_DEBUG_TEMP, "DONE!\n");

	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void set_vbus_from_u3phy(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);

	set_vbus_sel_mode(hiusbc, HIUSBC_VBUS_U3);
	set_u3_vbus_sel(hiusbc, HIUSBC_U3_VBUS_FROM_PIPE);

	hiusbc_dbg(HIUSBC_DEBUG_TEMP, "DONE!\n");

	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void set_vbus_from_u3force_on(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);

	set_vbus_sel_mode(hiusbc, HIUSBC_VBUS_U3);
	set_u3_vbus_sel(hiusbc, HIUSBC_U3_VBUS_FORCE);
	set_u3_vbus_force_value(hiusbc, 1);

	hiusbc_dbg(HIUSBC_DEBUG_TEMP, "DONE!\n");

	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

STATIC void set_vbus_from_u3force_off(struct hiusbc *hiusbc)
{
	unsigned long flags;

	spin_lock_irqsave(&hiusbc->lock, flags);

	set_vbus_sel_mode(hiusbc, HIUSBC_VBUS_U3);
	set_u3_vbus_sel(hiusbc, HIUSBC_U3_VBUS_FORCE);
	set_u3_vbus_force_value(hiusbc, 0);

	hiusbc_dbg(HIUSBC_DEBUG_TEMP, "DONE!\n");

	spin_unlock_irqrestore(&hiusbc->lock, flags);
}

static const struct hiusbc_debugfs_cb phy_option[] = {
	{.string = "clk_lock", .cb = check_phy_clk_lock_rt},
	{.string = "clk_freq", .cb = check_phy_clk_freq},
	{.string = "mode_u2only", .cb = set_phy_u2only},
	{.string = "mode_u3only", .cb = set_phy_u3only},
	{.string = "mode_u2u3", .cb = set_phy_u2u3},
	{.string = "vbus_soc", .cb = set_vbus_from_soc},
	{.string = "vbus_u2phy", .cb = set_vbus_from_u2phy},
	{.string = "vbus_u2force_on", .cb = set_vbus_from_u2force_on},
	{.string = "vbus_u2force_off", .cb = set_vbus_from_u2force_off},
	{.string = "vbus_u3phy", .cb = set_vbus_from_u3phy},
	{.string = "vbus_u3force_on", .cb = set_vbus_from_u3force_on},
	{.string = "vbus_u3force_off", .cb = set_vbus_from_u3force_off},
};

STATIC ssize_t hiusbc_phy_write(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct hiusbc *hiusbc = s->private;
	u32 i;
	char buf[MAX_BUF_SIZE] = {0};

	if (copy_from_user(buf, ubuf, min_t(size_t, sizeof(buf) - 1, count))) {
		hiusbc_fs_err("set phy option fail!\n");
		return -EFAULT;
	}

	for (i = 0; i < ARRAY_SIZE(phy_option); i++) {
		if (!strncmp(buf, phy_option[i].string,
			strlen(phy_option[i].string))) {
			phy_option[i].cb(hiusbc);
			hiusbc_fs_info("set phy option success!\n");
			return count;
		}
	}
	hiusbc_fs_err("set phy option fail!\n");
	return -EINVAL;
}

static const struct file_operations hiusbc_phy_fops = {
	.open			= hiusbc_phy_open,
	.write			= hiusbc_phy_write,
	.read			= seq_read,
	.llseek			= seq_lseek,
	.release		= single_release,
};
#endif

void hiusbc_debugfs_init(struct hiusbc *hiusbc)
{
	struct dentry *root = NULL;
	struct dentry *file = NULL;
	char name[DEBUGFS_ROOT_NAME_LEN] = {0};

	if (sprintf_s(name, DEBUGFS_ROOT_NAME_LEN, "hiusbc%u", hiusbc->usb_id) == -1) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "debugfs init call sprintf_s failed!\n");
		return;
	}

	root = debugfs_create_dir(name, NULL);
	if (!root) {
		hiusbc_dbg(HIUSBC_DEBUG_ERR,
				"Can't create 'hiusbc' debugfs\n");
		return;
	}
	hiusbc->root = root;

	hiusbc->regset = kzalloc(sizeof(*hiusbc->regset), GFP_KERNEL);
	if (!hiusbc->regset) {
		debugfs_remove_recursive(root);
		hiusbc->root = NULL;
		return;
	}

	hiusbc->regset->regs = hiusbc_regs;
	hiusbc->regset->nregs = ARRAY_SIZE(hiusbc_regs);
	hiusbc->regset->base = hiusbc->regs;

	debugfs_create_regset32("regdump", 0400, root, hiusbc->regset);

	file = debugfs_create_file("log_level", 0600, root,
			hiusbc, &hiusbc_log_level_fops);
	if (!file)
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "Can't create 'log_level' debugfs\n");

	file = debugfs_create_file("mode", SYSFS_FILE_PERMISSION, root,
			hiusbc, &hiusbc_mode_fops);
	if (!file)
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "Can't create 'mode' debugfs\n");

	file = debugfs_create_file("link_state", SYSFS_FILE_PERMISSION,
			root, hiusbc, &hiusbc_link_state_fops);
	if (!file)
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "Can't create 'link_state' debugfs\n");

	file = debugfs_create_file("current_speed", SYSFS_FILE_PERMISSION, root,
			hiusbc, &hiusbc_current_speed_fops);
	if (!file)
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "Can't create 'current_speed' debugfs\n");

	file = debugfs_create_file("ltssm_dump", 0600, root,
			hiusbc, &hiusbc_ltssm_dump_fops);
	if (!file)
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "Can't create 'ltssm_dump' debugfs\n");
#ifdef CFG_BUILD_DEBUG
	file = debugfs_create_file("testmode", SYSFS_FILE_PERMISSION, root,
			hiusbc, &hiusbc_testmode_fops);
	if (!file)
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "Can't create 'testmode' debugfs\n");

	file = debugfs_create_file("maximum_speed", SYSFS_FILE_PERMISSION, root,
			hiusbc, &hiusbc_maximum_speed_fops);
	if (!file)
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "Can't create 'maximum_speed' debugfs\n");

	file = debugfs_create_file("cptest", SYSFS_FILE_PERMISSION, root,
			hiusbc, &hiusbc_cp_test_fops);
	if (!file)
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "Can't create 'cptest' debugfs\n");

	file = debugfs_create_file("lpm", SYSFS_FILE_PERMISSION, root,
			hiusbc, &hiusbc_lpm_fops);
	if (!file)
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "Can't create 'lpm' debugfs\n");

	file = debugfs_create_file("fpga_debug", SYSFS_FILE_PERMISSION, root,
			hiusbc, &hiusbc_fpga_debug_fops);
	if (!file)
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "Can't create 'fpga_debug' debugfs\n");

	file = debugfs_create_file("phy", SYSFS_FILE_PERMISSION, root,
			hiusbc, &hiusbc_phy_fops);
	if (!file)
		hiusbc_dbg(HIUSBC_DEBUG_ERR, "Can't create 'phy' debugfs\n");
#endif
}

void hiusbc_debugfs_exit(struct hiusbc *hiusbc)
{
	debugfs_remove_recursive(hiusbc->root);
	kfree(hiusbc->regset);
}
