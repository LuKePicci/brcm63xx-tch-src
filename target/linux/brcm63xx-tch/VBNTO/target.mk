SUBTARGET:=VBNTO
BOARDNAME:=VBNT-O

BRCM_CHIP:=63138
ARCH:=arm
CPU_TYPE:=cortex-a9
CPU_SUBTYPE:=neon

define Target/Description
	Broadcom 63x38 (VBNT-O)
endef

#BCM_SDK=$(TOPDIR)/extern/vant-f_telia_17.2.278_2_20180704
#BRCMDRIVERS_DIR:=$(BCM_SDK)/broadcom_modules/bcmdrivers
#BCM_USERSPACE_DIR:=$(BCM_SDK)/bcm_userspace
#SHARED_DIR:=$(BCM_SDK)/bcm_userspace/broadcom_apps/shared

### experimenting with non-tch newer SDKs
BCM_SDK=$(TOPDIR)/extern/broadcom-sdk-416L05-zyxel-vmg8823-b50b
BRCMDRIVERS_DIR:=$(BCM_SDK)/bcmdrivers
BCM_USERSPACE_DIR:=
SHARED_DIR:=$(BCM_SDK)/shared
RDP_PATH:=

PROFILE:=VMG8823_B50B
RDPA_PLATFORM=dsl
VOXXXLOAD=1

INC_BROADCOM_APPS_SHARED:=$(BCM_USERSPACE_DIR)/broadcom_apps/shared
INC_BRCMDRIVER_PUB_PATH:=$(BRCMDRIVERS_DIR)/opensource/include
INC_BRCMDRIVER_PRIV_PATH:=$(BRCMDRIVERS_DIR)/broadcom/include
INC_ADSLDRV_PATH:=$(BRCMDRIVERS_DIR)/broadcom/char/adsl/impl1
INC_ATMAPI_DRV_PATH:=$(BRCMDRIVERS_DIR)/broadcom/char/atmapi/impl1
INC_MOCACFGDRV_PATH:=$(BRCMDRIVERS_DIR)/opensource/char/moca/impl2
INC_BRCMSHARED_PUB_PATH:=$(SHARED_DIR)/opensource/include
INC_BRCMSHARED_PRIV_PATH:=$(SHARED_DIR)/broadcom/include
INC_BRCMBOARDPARMS_PATH:=$(SHARED_DIR)/opensource/boardparms
INC_FLASH_PATH:=$(SHARED_DIR)/opensource/flash
INC_SPI_PATH:=$(SHARED_DIR)/opensource/spi
INC_SPUDRV_PATH:=$(BRCMDRIVERS_DIR)/opensource/char/spudd/impl3
INC_PWRMNGTDRV_PATH:=$(BRCMDRIVERS_DIR)/broadcom/char/pwrmngt/impl1
INC_ENETDRV_PATH:=$(BRCMDRIVERS_DIR)/opensource/net/enet/impl5
INC_EPONDRV_PATH:=$(BRCMDRIVERS_DIR)/broadcom/char/epon/impl1
INC_RDPA_PATH:=$(BRCMDRIVERS_DIR)/opensource/char/rdpa_gpl/impl1/include
INC_RDPA_EXT_PATH:=$(BRCMDRIVERS_DIR)/opensource/char/rdpa_gpl_ext/impl1/include
INC_RDPA_MW_PATH:=$(BRCMDRIVERS_DIR)/opensource/char/rdpa_mw/impl1
INC_RDPA_DRV_PATH:=$(BRCMDRIVERS_DIR)/opensource/char/rdpa_drv/impl1
INC_RDPA_PATH_PLATFORM:=$(INC_RDPA_PATH)/$(RDPA_PLATFORM)
INC_RDPA_EXT_PATH_PLATFORM:=$(INC_RDPA_EXT_PATH)/$(RDPA_PLATFORM)
INC_BDMF_PATH:=$(BRCMDRIVERS_DIR)/opensource/char/bdmf/impl1
INC_UTILS_PATH:=$(SHARED_DIR)/opensource/utils

EXTRA_CFLAGS_BDMF=-I$(INC_BDMF_PATH)/framework \
                  -I$(INC_BDMF_PATH)/platform/$(RDPA_PLATFORM) \
                  -I$(INC_BDMF_PATH)/system \
                  -I$(INC_BDMF_PATH)/system/linux \
                  -I$(INC_BDMF_PATH)/system/linux/ce \
                  -I$(INC_BDMF_PATH)/shell

EXTRA_CFLAGS_RDPA=$(EXTRA_CFLAGS_BDMF) \
                  -I$(INC_RDPA_PATH) \
                  -I$(INC_RDPA_EXT_PATH) \
                  -I$(INC_RDPA_PATH)/$(RDPA_PLATFORM) \
                  -I$(INC_RDPA_EXT_PATH)/$(RDPA_PLATFORM) \
                  -I$(INC_RDPA_MW_PATH) \
                  -I$(INC_RDPA_DRV_PATH)

ifneq ($(strip $(RDPA_PLATFORM)),)
EXTRA_CFLAGS_RDPA += -DRDPA_PLATFORM
endif

ADD_LINUX_INCLUDE:=-I$(BRCMDRIVERS_DIR)/broadcom/net/wl/impl22/main/src/include

INC_ENET_DMA_FLAGS = -I$(INC_BRCMSHARED_PUB_PATH)/drv/phys \
                     -I$(INC_BRCMSHARED_PUB_PATH)/bcm963xx \
                     -I$(BRCMDRIVERS_DIR)/opensource/include/bcm963xx \
                     -I$(BRCMDRIVERS_DIR)/opensource/net/enet/shared \
                     -I$(BRCMDRIVERS_DIR)/broadcom/include/bcm963xx

export BCM_SDK BRCMDRIVERS_DIR BCM_USERSPACE_DIR SHARED_DIR \
       BRCM_CHIP PROFILE RDPA_PLATFORM VOXXXLOAD ADD_LINUX_INCLUDE \
       INC_BROADCOM_APPS_SHARED INC_BRCMDRIVER_PUB_PATH INC_BRCMDRIVER_PRIV_PATH \
       INC_ADSLDRV_PATH INC_ATMAPI_DRV_PATH INC_MOCACFGDRV_PATH INC_BRCMSHARED_PUB_PATH \
       INC_BRCMSHARED_PRIV_PATH INC_BRCMBOARDPARMS_PATH INC_FLASH_PATH INC_UTILS_PATH \
       INC_SPI_PATH INC_SPUDRV_PATH INC_PWRMNGTDRV_PATH INC_ENETDRV_PATH INC_EPONDRV_PATH \
       INC_RDPA_PATH INC_RDPA_EXT_PATH \
       INC_RDPA_MW_PATH INC_RDPA_DRV_PATH \
       INC_RDPA_PATH_PLATFORM INC_RDPA_EXT_PATH_PLATFORM \
       INC_BDMF_PATH RDP_PATH INC_ENET_DMA_FLAGS \
       EXTRA_CFLAGS_RDPA EXTRA_CFLAGS_BDMF


