KERNEL_DIR :=$(srctree)

EXTRA_CFLAGS += -I$(DRIVER_KERNEL_DIR)/inc
EXTRA_CFLAGS += -I$(DRIVER_CHIP_KERNEL_DIR)/src/pcie_rc_sr/
EXTRA_CFLAGS += -I$(DRIVER_CHIP_KERNEL_DIR)/src/pcie_rc_sr/soc
EXTRA_CFLAGS += -I$(TOP_DIR)/abl/libc_sec/include
EXTRA_CFLAGS += -I$(TOP_DIR)/inc/driver
EXTRA_CFLAGS += -fstack-protector-all
#EXTRA_CFLAGS += -DSNAPSHOT_ENABLE
EXTRA_CFLAGS += -DNOT_SUPPORT_OSNAME
EXTRA_CFLAGS += -DCFG_ENV_PCIE_RC_SR

ccflags-y += -Wall -Werror -fno-common -Wfloat-equal -Wformat=2 $(WDATE_TIME) -Wcast-align  -Wundef -Wstrict-prototypes
ccflags-y += -Wunused -O2 -Wextra -Wno-unused-parameter -Wno-sign-compare

PCIE_RC_DRV_OBJS := pcie_rc_drv.o drv_snapshot.o pcie_qos_common.o pcie_ras_config.o pcie_rc_qos.o pcie_rc_ctrl.o

#ifneq ($(filter $(PRODUCT),  ascend310B ascend310Brc as31xm1),)
#    EXTRA_CFLAGS += -I$(DRIVER_CHIP_KERNEL_DIR)/src/pcie_rc_sr/soc/mini_v3
#    PCIE_RC_DRV_OBJS += soc/mini_v3/pcie_rc_ctrl.o
#else ifneq ($(filter $(PRODUCT),  ascend610Lite ascend610Liteesl),)
#    EXTRA_CFLAGS += -I$(DRIVER_CHIP_KERNEL_DIR)/src/pcie_rc_sr/soc/mdc_v2
#    PCIE_RC_DRV_OBJS += soc/mdc_v2/pcie_rc_ctrl.o soc/mdc_v2/pcie_port_init.o
#endif

ifneq ($(filter $(PRODUCT), as31xm1),)
    EXTRA_CFLAGS += -DCFG_FEATURE_PCIE_DFX
    EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MDC_V11
    EXTRA_CFLAGS += -DCFG_FEATURE_SERDES_H25
    EXTRA_CFLAGS += -I$(TOP_DIR)/drivers/ai_sdk/driver/linux/kernel_space/src/serdes_drv/serdes_h25/includes
    PCIE_RC_DRV_OBJS += pcie_rc_dfx.o
endif

ifneq ($(filter $(PRODUCT), ascend610Lite),)
    EXTRA_CFLAGS += -DCFG_SOC_PLATFORM_MDC_V51_LITE
endif

obj-y += drv_pcierc_sr.o
drv_pcierc_sr-objs := $(PCIE_RC_DRV_OBJS)
