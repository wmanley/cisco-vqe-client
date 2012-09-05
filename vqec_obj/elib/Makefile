#
# Copyright (c) 2006-2007 by Cisco Systems, Inc.
# All Rights Reserved.
#

#always include vqe.mk, vqe_builds.mk and  vqec.mk first
include ../../vqe.mk
include ../../environ.mk
include ../../vqe_builds.mk
include ../vqec.mk

# Include the BUILD file for the component we want to build.
# This will first build dependencies and then the build target.
# In case or elib component build will be done only if HAVE_ELOG
# variable is set to 1.
ifeq ($(HAVE_ELOG), 1)
 include $(SRCDIR)/BUILD
endif
