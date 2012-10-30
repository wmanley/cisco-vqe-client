#
# Copyright (c) 2008 by Cisco Systems, Inc.
# All Rights Reserved.
#

###########
# Main Makefile for building kernel modules. The source for the kernel module
# is copied into $(ARCH)-dev with this directory as the base of the source.
# All objects are then built in the $(ARCH)-dev directory, with the final .ko 
# located in that directory. Helpful file descriptions:
#
# - Makefile.mod is the makefile executed from the kernel build system, and
#   is copied to $(ARCH)-dev as part of the build process.
# - MODBUILD.mk is a makefile located in the individual source components
#   which is used to define the sources that are built for the kernel module.
# - The name of the kernel module is in the variable "KMODNAME".
###########


include ../../vqe.mk
include ../../environ.mk

#
# Source directories (MODBUILD.mk files)
#
KMOD_SOURCE_DIRS =		\
	utils			\
	utils/log		\
	utils/mp_tlv		\
	rtp			\
	vqecutils		\
	eva/vqec-dp		\
	eva/vqec-dp/channel	\
	eva/vqec-dp/inputshim	\
	eva/vqec-dp/outputshim	\

#
# Source directory for test reader module
#
TEST_READER_DIR = eva/vqec-dp

#
# Common Include header directories.
#
KMOD_INCLUDE_DIRS =		\
	include/log		\
	include/utils		\
	add-ons/include		\

#
# directories from which to build the source
#
KMOD_SOURCEBUILD_DIRS = $(KMOD_SOURCE_DIRS) $(KMOD_INCLUDE_DIRS)


KMODNAME 		:= vqec_dp
KMODMAKEFILE	:= Makefile.kmod
VERSION_DIR		:= eva
VERSION_FILE	:= vqec_version.h

GENOBJDIR		= $(ROOT)/vqec_obj/kmod
GENINCLUDES 	= $(GENOBJDIR)/$(ARCH)-dev/vqecutils/vqec_rpc_common.h_rpcgen  \
			$(GENOBJDIR)/$(ARCH)-dev/eva/vqec-dp/vqec_dp_rpc_server.c_rpcgen

#
# Default target for building the kernel module.
#
all::	_cpsources  $(GENINCLUDES) _kbuild
	@if ! $(MAKE) -C `pwd`/$(ARCH)-dev KMODARCH=$(ARCH) all; then \
		exit 1; \
	fi
	echo "===> Building test reader module"; \
	echo "obj-m += test_vqec_reader.o" > `pwd`/$(ARCH)-dev/Kbuild.obj; \
	echo "test_vqec_reader-objs += ./eva/vqec-dp/test_vqec_reader.o" >> `pwd`/$(ARCH)-dev/Kbuild.obj; \
	$(MAKE) -C `pwd`/$(ARCH)-dev KMODARCH=$(ARCH) all; \


#
# Instance of the source-tree (c files, .h header files)
#
_cpsources:
	@if [ ! -e $(ROOT)/$(VERSION_DIR)/$(VERSION_FILE) ]; then	\
		echo "$(VERSION_FILE) does not exist";			\
		exit 1;								\
	else									\
		test -d `pwd`/$(ARCH)-dev/$(VERSION_DIR) || 		\
			mkdir -p `pwd`/$(ARCH)-dev/$(VERSION_DIR);	\
		cp -f -u $(ROOT)/$(VERSION_DIR)/$(VERSION_FILE) 	\
			`pwd`/$(ARCH)-dev/$(VERSION_DIR);		\
	fi;									\
	for dir in $(KMOD_SOURCEBUILD_DIRS); do				\
		echo "===> $$dir";						\
		if [ ! -e $(ROOT)/$$dir/MODBUILD.mk ]; then			\
			echo "$$dir MODBUILD.mk file not found";		\
			exit 1;							\
		else 								\
			$(MAKE) -C $(ROOT)/$$dir -f $(ROOT)/$$dir/MODBUILD.mk _cpsources \
				KMODOBJBASE=`pwd` KMODNAME=$(KMODNAME);		\
		fi;								\
	done;									\


#
# Kbuild specific-file which describes objects to be built for the module.
#
_kbuild:
	@test -d `pwd`/$(ARCH)-dev || 						\
		mkdir -p `pwd`/$(ARCH)-dev;			 		\
	$(RM) `pwd`/$(ARCH)-dev/Kbuild.obj;					\
	echo "obj-m += $(KMODNAME).o" > `pwd`/$(ARCH)-dev/Kbuild.obj;		\
	for dir in $(KMOD_SOURCE_DIRS); do					\
		echo "===> $$dir";						\
		if [ ! -e $(ROOT)/$$dir/MODBUILD.mk ]; then			\
			echo "MODBUILD file not found";				\
		else 								\
			$(MAKE) -C $(ROOT)/$$dir -f $(ROOT)/$$dir/MODBUILD.mk _kbuild	\
				KMODOBJBASE=`pwd` KMODNAME=$(KMODNAME);		\
		fi;								\
	done;									\
	echo "[cp] =====> `pwd`/$(ARCH)-dev$(SRCMODULE)/$(KMODMAKEFILE)";	\
	cp -f -u $(KMODMAKEFILE) `pwd`/$(ARCH)-dev/Makefile;			\

#
# Generate server RPC sources in appropriate locations.
# vqec_rpc_common.h_rpcgen in vqecutils
# vqec_dp_rpc_server.c_rpcgen in eva/vqec-dp
#
$(GENOBJDIR)/$(ARCH)-dev/vqecutils/vqec_rpc_common.h_rpcgen: $(ROOT)/eva/vqec-dp/vqec_dp_api.h \
		$(ROOT)/eva/vqec_rpcgen.pl $(ROOT)/eva/vqec-dp/vqec_dp_api_types.h
	@test -d `pwd`/$(ARCH)-dev/vqecutils || mkdir `pwd`/$(ARCH)-dev/vqecutils
	$(ROOT)/eva/vqec_rpcgen.pl -in $< -common $@ -module vqec;

$(GENOBJDIR)/$(ARCH)-dev/eva/vqec-dp/vqec_dp_rpc_server.c_rpcgen: $(ROOT)/eva/vqec-dp/vqec_dp_api.h \
		$(ROOT)/eva/vqec_rpcgen.pl $(ROOT)/eva/vqec-dp/vqec_dp_api_types.h
	@test -d `pwd`/$(ARCH)-dev/eva/vqec-dp || mkdir `pwd`/$(ARCH)-dev/eva/vqec-dp
	$(ROOT)/eva/vqec_rpcgen.pl -in $< -server $@ -module vqec;


#
# Clean the kernel module build.
#
clean::
	$(RM) -r $(ARCH)-dev






