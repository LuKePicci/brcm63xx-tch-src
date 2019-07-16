obj-$(CONFIG_BCM_WLAN) += dhd.o 
dhd-objs += dhd_dep.o

$(obj)/dhd_dep.o:
	cp $(obj)/dhd$(BCM_PROFILE).o_save $(obj)/dhd_dep.o
	if [ ! -d $(BCM_INSTALL_DIR)/etc/wlan/dhd ]; then \
		rm -f $(BCM_INSTALL_DIR)/etc/wlan/dhd; \
		install -d $(BCM_INSTALL_DIR)/etc/wlan/dhd; \
	fi;
	for x in $(obj)/*map.bin_save; do \
		cp $$x $${x%_save}; \
		install -m 755 $${x%_save} $(BCM_INSTALL_DIR)/etc/wlan; \
	done
	for x in $(obj)/*vars.bin_save; do \
		cp $$x $${x%_save}; \
		install -m 755 $${x%_save} $(BCM_INSTALL_DIR)/etc/wlan; \
	done
obj-$(CONFIG_BCM_WLAN) += wl.o
wl-objs += wl_dep.o

$(obj)/wl_dep.o:
	cp $(obj)/wl$(BCM_PROFILE).o_save $(obj)/wl_dep.o
	if [ ! -d $(BCM_INSTALL_DIR)/etc/wlan ]; then \
		rm -f $(BCM_INSTALL_DIR)/etc/wlan; \
		install -d $(BCM_INSTALL_DIR)/etc/wlan; \
	fi;
	for x in $(obj)/*map.bin_save; do \
		cp $$x $${x%_save}; \
		install -m 755 $${x%_save} $(BCM_INSTALL_DIR)/etc/wlan; \
	done
	for x in $(obj)/*vars.bin_save; do \
		cp $$x $${x%_save}; \
		install -m 755 $${x%_save} $(BCM_INSTALL_DIR)/etc/wlan; \
	done
obj-$(CONFIG_BCM_WLAN) += wlemf.o 
wlemf-objs += wlemf_dep.o

$(obj)/wlemf_dep.o:
	cp $(obj)/wlemf$(BCM_PROFILE).o_save $(obj)/wlemf_dep.o
