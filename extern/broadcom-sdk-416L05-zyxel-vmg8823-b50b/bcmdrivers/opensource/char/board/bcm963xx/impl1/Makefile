# Makefile for the bcm963xx board drivers
#


obj-y := board.o bcm63xx_flash.o bcm63xx_led.o bcm63xx_gpio.o pushbutton.o

ifneq ($(strip $(BUILD_IEEE1905)),)
obj-y += ieee1905_module.o    
endif

ifneq ($(findstring _$(strip $(BRCM_CHIP))_,,_63268_63138_63148_6838_),)

ifneq ($(strip $(BUILD_6802_MOCA)),)
    obj-y += bbsi/bbsi.o    
    obj-y += bbsi/bbsiExecSeq.o   
    obj-y += moca/6802/init.o   
    
else
    obj-y += spidevices.o
endif

endif

ifneq ($(strip $(CONFIG_BCM_CPLD1)),)
    obj-y += bcmcpld1.o
	EXTRA_CFLAGS += -DCONFIG_BCM_CPLD1
endif

ifneq ($(strip $(CONFIG_MTD_NAND)),)
	EXTRA_CFLAGS += -DBUILD_NAND
endif

ifneq ($(strip $(CONFIG_BCM_PLC_BOOT)),)
    obj-y += homeplugd_module.o
endif

ifneq ($(strip $(subst ", ,$(BUILD_BRCM_XDSL_DISTPOINT))),)
    obj-y += dsldsp_operation.o
    obj-y += xdsl_distpoint.o
	EXTRA_CFLAGS += -DBRCM_XDSL_DISTPOINT
endif
ifneq ($(strip $(BUILD_BRCM_FTTDP)),)
	EXTRA_CFLAGS += -DBRCM_FTTDP
endif

# 20130822, TimLiu add ZyXEL compile flag
ifeq ($(strip $(ZYXEL)),y)
EXTRA_CFLAGS += -DZYXEL=1
ifneq ("$(strip ${ZYXEL_PRODUCT})","")
EXTRA_CFLAGS += -DZYPRJ_$(ZYXEL_PRODUCT)=1
endif
ifeq ($(strip ${ZYXEL_PARTITION}),y)
EXTRA_CFLAGS += -DZYXEL_PARTITION=1
endif
ifeq ($(strip ${ZYXEL_EXTERNAL_DECT}),y)
EXTRA_CFLAGS += -DZYXEL_EXTERNAL_DECT=1
endif
ifeq ($(strip ${ZYXEL_KERNEL_RESET_BTN}),y)
EXTRA_CFLAGS += -DZYXEL_KERNEL_RESET_BTN=1
endif
ifneq ($(strip ${ZYPRJ_HARDWARE_VERSION}),)
EXTRA_CFLAGS += -DZYPRJ_HARDWARE_VERSION=$(ZYPRJ_HARDWARE_VERSION)
endif
ifneq ($(strip ${ZYXEL_WIFI_LED_PATTERN}),)
EXTRA_CFLAGS += -DZYXEL_WIFI_LED_PATTERN=$(ZYXEL_WIFI_LED_PATTERN)
endif
#__ZyXEL__, David, support SW Internet LED blinking
ifeq ($(strip ${ZYXEL_SW_INTERNET_LED_BLINK}),y)
EXTRA_CFLAGS += -DZYXEL_SW_INTERNET_LED_BLINK
endif
#__ZyXEL__, JoannaSu, support FW Upgrade Power LED blinking
ifeq ($(strip ${ZYXEL_FW_UPGRADE_POWER_LED_BLINK}),y)
EXTRA_CFLAGS += -DZYXEL_FW_UPGRADE_POWER_LED_BLINK
endif
# Zyxel Feature Bits
ifeq ($(strip ${ZYXEL_FEATURE_BITS}),y)
EXTRA_CFLAGS += -DZYXEL_FEATURE_BITS
endif
#__ZyXEL__, for ZyXEL hardware depend misc.
ifeq ($(strip ${ZYXEL_QTN_WIFI_5G_SUPPORT}),y)
EXTRA_CFLAGS += -DZYXEL_QTN_WIFI_5G_SUPPORT
endif
ifeq ($(strip ${ZYXEL_QTN_SW_RESET}),y)
EXTRA_CFLAGS += -DZYXEL_QTN_SW_RESET
endif
ifeq ($(strip ${ZYXEL_WPS_SPOTLIGHT}),y)
EXTRA_CFLAGS += -DZYXEL_WPS_SPOTLIGHT
ifeq ($(strip ${ZYXEL_WPS_SPOTLIGHT_2GPIO}),y)
EXTRA_CFLAGS += -DZYXEL_WPS_SPOTLIGHT_2GPIO
endif
endif
ifeq ($(strip ${ZYXEL_SFP_MODULE_SUPPORT}),y)
EXTRA_CFLAGS += -DZYXEL_SFP_MODULE_SUPPORT
endif
ifeq ($(strip ${ZYXEL_SFP_DETECT_PIN}),y)
EXTRA_CFLAGS += -DZYXEL_SFP_DETECT_PIN
endif
ifeq ($(strip ${ZYXEL_SFP_TX_FAULT_PIN}),y)
EXTRA_CFLAGS += -DZYXEL_SFP_TX_FAULT_PIN
endif
ifeq ($(strip ${ZYXEL_SFP_TX_DISABLE}),y)
EXTRA_CFLAGS += -DZYXEL_SFP_TX_DISABLE
endif
ifeq ($(strip ${ZYXEL_ZIGBEE_SUPPORT}),y)
EXTRA_CFLAGS += -DZYXEL_ZIGBEE_SUPPORT
endif
ifeq ($(strip ${ZYXEL_BOARD_DETECT_BY_GPIO}),y)
EXTRA_CFLAGS += -DZYXEL_BOARD_DETECT_BY_GPIO
endif
# Lan Aggregate Led support
ifeq ($(strip ${ZYXEL_SUPPORT_SW_LAN_AGGREGATE_LED}),y)
EXTRA_CFLAGS += -DZYXEL_SUPPORT_SW_LAN_AGGREGATE_LED
endif

ifeq ($(strip $(SAME_FW_FOR_TWO_BOARD)),y)
EXTRA_CFLAGS += -DSAME_FW_FOR_TWO_BOARD
endif

endif

ifeq ($(BUILD_IEEE1905),y)
	EXTRA_CFLAGS += -DSUPPORT_IEEE1905
endif
 
ifeq ($(strip $(BRCM_CHIP)),63268)
ifneq ($(strip $(BUILD_SECURE_BOOT)),)
ifeq ($(strip $(BUILD_NVRAM_3K)),y)
	EXTRA_CFLAGS += -DINC_BTRM_BOOT=1
endif
endif
endif
 
EXTRA_CFLAGS += -I. -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD)
ifneq ($(findstring _$(strip $(BRCM_CHIP))_,,_63138_63148_6838_63381_),)
EXTRA_CFLAGS += -I$(INC_BRCMSHARED_PUB_PATH)/pmc
endif
EXTRA_CFLAGS += -Werror -Wfatal-errors
EXTRA_CFLAGS += -g

ifneq ($(strip $(CONFIG_BCM_WLAN)),)
	EXTRA_CFLAGS += -DWIRELESS
endif

ifeq ($(strip ${ZYXEL_FIX_WAN_MAC}),y)
EXTRA_CFLAGS += -DFIX_WAN_MAC
endif

ifeq ($(strip ${ZYXEL_L2_WAN_SAME_MAC}),y)
EXTRA_CFLAGS += -DZYXEL_L2_WAN_SAME_MAC
endif

ifeq ($(strip ${ZYXEL_L3_WAN_SAME_MAC}),y)
EXTRA_CFLAGS += -DZYXEL_L3_WAN_SAME_MAC
endif

-include $(TOPDIR)/Rules.make

clean:
	rm -f core *.o *.a *.s

