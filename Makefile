#
# Copyright (c) 2006-2009 by Cisco Systems, Inc.
# All Rights Reserved.
#
#
# This is the VQE-C top level Makefile which builds the VQE-C library
# and sample application.  This Makefile basically descends into each
# subdirectory and calls the make from there.
#
#

# Define global variables from vqe.mk
include vqe.mk

VQEC_BUILD_ROOT=  $(ROOT)/vqec_obj
LIBEVENT_SRCDIR=  3rd-party-src/libevent/libevent-1.1a/
LIBCONFIG_SRCDIR= 3rd-party-src/libconf/libconfig-1.0.1/

# VQEC targets
vqec::
	@echo "Building 3rd-party libs"; \
	if ! $(MAKE) -wC vqec_obj vqec_build_packages; then \
		$(MAKE_K_CHECK) \
	fi;

	@echo "Making VQEC libs and executables"; \
	if ! $(MAKE) -wC $(VQEC_BUILD_ROOT)$(SRCMODULE); then \
		$(MAKE_K_CHECK) \
	fi;

%-vqec::
	@echo "Building 3rd-party libs"; \
	if ! $(MAKE) -wC vqec_obj vqec_build_packages; then \
		$(MAKE_K_CHECK) \
	fi;
	@echo "Making VQEC $*"; \
	if ! $(MAKE) -wC $(VQEC_BUILD_ROOT)$(SRCMODULE) $*; then \
		$(MAKE_K_CHECK) \
	fi

##########
# Kernel module targets for VQEC.
#########
kmod-vqec::
	@echo "Making VQEC kernel module"; \
        if ! $(MAKE) -wC $(VQEC_BUILD_ROOT) kmod; then \
                $(MAKE_K_CHECK) \
        fi

%-kmod-vqec::
	@echo "Making VQEC kernel module target $*-kmod"; \
        if ! $(MAKE) -wC $(VQEC_BUILD_ROOT) $*-kmod; then \
                $(MAKE_K_CHECK) \
        fi

# Build target
all::  vqec

# clean_release cleans all VQEC targets as well as the 3rd-party-src
clean::
	@echo "Cleaning VQEC and 3rd-party-src"; \
	if ! $(MAKE) -wC $(VQEC_BUILD_ROOT) clean_release; then \
		$(MAKE_K_CHECK) \
	fi 

.PHONY: install
install:
	@echo "installing library and headers";\
	if ! $(MAKE) -wC vqec_obj vqec_install; then \
		$(MAKE_K_CHECK) \
	fi;

	@echo "installing .pc files"; \
	$(MAKE) -wC pkgconfig install
