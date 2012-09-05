#
# Copyright (c) 2008 by Cisco Systems, Inc.
# All rights reserved.
#

#
# This file contains variables used by the VQE-C dataplane Makefiles:
#   Makefile
#   inputshim/Makefile
#   outputshim/Makefile
#   channel/Makefile
#

#
# local include paths go here
#
INCLUDE_PATHS += $(ROOT)/eva/vqec-dp 	\
	$(ROOT)/eva/vqec-dp/inputshim	\
	$(ROOT)/eva/vqec-dp/outputshim	\
	$(ROOT)/eva/vqec-dp/channel	\
	$(MODOBJ)			\
	$(ROOT)			\
	$(ROOT)/eva			\
	$(ROOT)/vqecutils		\
	$(ROOT)/include			\
	$(ROOT)/include/utils		\
	$(ROOT)/include/log		\
	$(ROOT)/add-ons/include		\
	$(ROOT)/rtp
	

INCLUDES = $(addprefix -I, $(INCLUDE_PATHS))

#
# Particular flag for dataplane.
#
CFLAGS += -D__VQEC_DP__

#
# Have syslog support
#
ifeq ($(HAVE_SYSLOG), 1)
 CFLAGS += -DSYS_HAS_SYSLOG
endif

