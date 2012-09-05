#
# Copyright (c) 2006-2007 by Cisco Systems, Inc.
# All rights reserved.
#

#
# This file contains variables used by the VQE-C Makefiles:
#   Makefile
#   sample/Makefile
#   utest/Makefile
#

#local include paths go here
INCLUDE_PATHS += $(SRCDIR) 	\
	$(ROOT)			\
	$(ROOT)/vqecutils		\
	$(ROOT)/eva		\
	$(ROOT)/utils 		\
	$(ROOT)/include	\
	$(ROOT)/include/utils 	\
	$(ROOT)/include/log	\
        $(ROOT)/add-ons/include \
	$(ROOT)/cfg	\
	$(ROOT)/rtp	\
	$(ROOT)/elib

INCLUDES = $(addprefix -I, $(INCLUDE_PATHS))

LDFLAGS = -L$(LIBDIR) -L$(ROOT)/add-ons/lib/$(ARCH)

#
# Internal dependencies (libs built in our domain)
#
VQECUTILS_DEP_VQE_LIBS =			\
	libdebug_vqec.a			\
	libzonemgr.a 			\

VQECUTILS_DEP_ADD_ON_LIBS =			\
	libevent.a			\

VQECUTILS_DEP_VQE = 	$(patsubst %, $(LIBDIR)/%, $(VQECUTILS_DEP_VQE_LIBS)) \
		$(patsubst %, $(ROOT)/add-ons/lib/$(ARCH)/%, $(VQECUTILS_DEP_ADD_ON_LIBS))

ifeq ($(HAVE_SYSLOG), 1)
 CFLAGS += -DSYS_HAS_SYSLOG
endif

#
# Locations of compiled libraries for VQE-C utils
#
VQECUTILS_LIB 	 	= $(LIBDIR)/libvqecutils.a

#
# Keep make from deleting intermediate files.
#
sources: $(VQECUTILS_LIB_OBJ)

