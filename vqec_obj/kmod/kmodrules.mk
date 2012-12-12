#
# Copyright (c) 2008 by Cisco Systems, Inc.
# All Rights Reserved.
#


##########
# Kernel module build rules for copying sources / adding module object to 
# the Kbuild.obj file.
##########

_kbuild::
	@test -d $(KMODOBJBASE)/$(ARCH)-dev ||		 			\
		(echo "arch directory does not exist"; exit 1);			\
	test -f $(KMODOBJBASE)/$(ARCH)-dev/Kbuild.obj ||		 	\
		(echo "Kbuikd file does not exist"; exit 1);			\
	echo -n "$(KMODNAME)-objs +=" >> $(KMODOBJBASE)/$(ARCH)-dev/Kbuild.obj;	\
	echo $(KMODOBJECTS) >> $(KMODOBJBASE)/$(ARCH)-dev/Kbuild.obj;		\


_cpsources::
	@mkdir -p $(KMODOBJBASE)/$(ARCH)-dev$(SRCMODULE); 		\
	for cfile in $(COMMON_SOURCES); do						\
		echo "[cp] =====> $(KMODOBJBASE)/$(ARCH)-dev$(SRCMODULE)/$$cfile"; \
		cp -f -u $$cfile $(KMODOBJBASE)/$(ARCH)-dev$(SRCMODULE)  >& /dev/null;	\
	done;									\
	for cfile in $(KERNELPORT_SOURCES); do					\
		echo "[cp] =====> $(KMODOBJBASE)/$(ARCH)-dev$(SRCMODULE)/`echo $$cfile | sed 's/kmod[.\/]//g'`"; \
		cp -f -u $$cfile $(KMODOBJBASE)/$(ARCH)-dev$(SRCMODULE)/`echo $$cfile | sed 's/kmod[.\/]//g'`; \
	done;									\
	for hfile in $(COMMON_HEADERS); do						\
		echo "[cp] =====> $(KMODOBJBASE)/$(ARCH)-dev$(SRCMODULE)/$$hfile";	\
		cp -f -u $$hfile $(KMODOBJBASE)/$(ARCH)-dev$(SRCMODULE)/$$hfile  >& /dev/null;	\
	done;									\
	for hfile in $(KERNELPORT_HEADERS); do					\
		echo "[cp] =====> $(KMODOBJBASE)/$(ARCH)-dev$(SRCMODULE)/`echo $$hfile | sed 's/kmod[.\/]//g'`"; \
		cp -f -u $$hfile $(KMODOBJBASE)/$(ARCH)-dev$(SRCMODULE)/`echo $$hfile | sed 's/kmod[.\/]//g'`; \
	done;									\
