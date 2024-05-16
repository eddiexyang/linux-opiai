KERNEL_DIR := $(srctree)
ccflags-y += -Wall -Werror -Wno-implicit-fallthrough
EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver
EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/kernel/hiusbc/include/
EXTRA_CFLAGS += -I$(TOP_DIR)/abl/libc_sec/include
EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/arc/linux/kernel_space/inc/
drv_hiusbc-objs += hiusbc_core.o hiusbc_debugfs_common.o hiusbc_debugfs.o hiusbc_drd.o hiusbc_setup.o hiusbc_transfer.o hiusbc_event.o hiusbc_gadget.o hiusbc_host.o hiusbc_asic_config.o
obj-$(CONFIG_USB_HIUSBC)  += drv_hiusbc.o

#ifeq ($(TARGET_BUILD_TYPE),debug)
#        EXTRA_CFLAGS += -DCFG_BUILD_DEBUG
#endif
