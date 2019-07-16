# Note: CONFIG_MIPS_BRCM is no longer defined with the new kernel...
# CONFIG_BCM_IN_KERNEL is not defined with the new...
# Ifs in Makefiles can’t do Ors...
# So cat strings together, and if one of them is 'y' ...
ifeq ($(CONFIG_MIPS_BRCM)$(CONFIG_BCM_IN_KERNEL),y)

# Linux
obj-y		+= boardparms.o bp_funcs.o

ifneq ($(strip $(VOXXXLOAD)),)
obj-y		+= boardparms_voice.o
EXTRA_CFLAGS += -DCONFIG_BOARDPARMS_VOICE
endif

# 20130821, TimLiu add new board parameter
ifeq ($(strip $(ZYXEL)),y)
EXTRA_CFLAGS += -DZYXEL=1
ifneq ("$(strip ${ZYXEL_PRODUCT})","")
EXTRA_CFLAGS += -DZYPRJ_$(ZYXEL_PRODUCT)=1
endif
ifeq ($(strip ${ZYXEL_PARTITION}),y)
EXTRA_CFLAGS += -DZYXEL_PARTITION=1
endif
ifneq ($(strip ${ZYPRJ_HARDWARE_VERSION}),)
EXTRA_CFLAGS += -DZYPRJ_HARDWARE_VERSION=$(ZYPRJ_HARDWARE_VERSION)
endif
ifeq ($(strip ${ZYXEL_EXTERNAL_DECT}),y)
EXTRA_CFLAGS += -DZYXEL_EXTERNAL_DECT=1
endif
ifneq ($(strip ${ZYXEL_WIFI_LED_PATTERN}),)
EXTRA_CFLAGS += -DZYXEL_WIFI_LED_PATTERN=$(ZYXEL_WIFI_LED_PATTERN)
endif
ifneq ($(strip ${ZYXEL_SUPPORT_ETHWAN_AS_LAN}),)
EXTRA_CFLAGS += -DZYXEL_SUPPORT_ETHWAN_AS_LAN=$(ZYXEL_SUPPORT_ETHWAN_AS_LAN)
endif
## FEM detection
ifeq ($(strip $(ZYXEL_FEM_DETECT)),y)
EXTRA_CFLAGS += -DZYXEL_FEM_DETECT
endif

ifeq ($(strip $(SAME_FW_FOR_TWO_BOARD)),y)
EXTRA_CFLAGS += -DSAME_FW_FOR_TWO_BOARD
endif

## Zyxel Feature Bits
ifeq ($(strip $(ZYXEL_FEATURE_BITS)),y)
EXTRA_CFLAGS += -DZYXEL_FEATURE_BITS
endif
endif

ifeq ($(strip $(ZYXEL_USE_CE_SROM_FOR_ELISA_PROJECT)),y)
EXTRA_CFLAGS += -DZYXEL_USE_CE_SROM_FOR_ELISA_PROJECT
endif

ifeq ($(strip $(ZYXEL_WIND_ITALY_CUSTOMIZATION)),y)
EXTRA_CFLAGS += -DZYXEL_WIND_ITALY_CUSTOMIZATION
endif

ifeq ($(strip $(ZYXEL_ISKON_CUSTOMIZATION)),y)
EXTRA_CFLAGS += -DZYXEL_ISKON_CUSTOMIZATION
endif

ifeq ($(strip $(ZYXEL_FORCE_WIFI_SROM)),y)
EXTRA_CFLAGS += -DZYXEL_FORCE_WIFI_SROM
endif

ifeq ($(strip $(ZYXEL_BOARD_DETECT_BY_GPIO)),y)
EXTRA_CFLAGS += -DZYXEL_BOARD_DETECT_BY_GPIO
endif

EXTRA_CFLAGS += -DCONFIG_BCM9$(BRCM_CHIP) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD)

ifeq ($(MOCA_LAN_DISCONN),y)
EXTRA_CFLAGS += -DCONFIG_NO_MOCA_LAN
endif

ifeq ($(MOCA_WAN_DISCONN),y)
EXTRA_CFLAGS += -DCONFIG_NO_MOCA_WAN
endif

-include $(TOPDIR)/Rules.make

endif


