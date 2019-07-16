obj-$(CONFIG_BCM_ADSL) += adsldd.o

adsldd-objs += adsldd_dep.o

$(obj)/adsldd_dep.o:
	cp $(obj)/adsldd$(BCM_PROFILE).o_save $(obj)/adsldd_dep.o
	cp $(obj)/adsl_phy$(BCM_PROFILE).bin_save $(obj)/adsl_phy.bin
	if [  -e  $(obj)/adsl_phy0$(BCM_PROFILE).bin_save ]; then \
		cp $(obj)/adsl_phy0$(BCM_PROFILE).bin_save $(obj)/adsl_phy0.bin; \
	fi



