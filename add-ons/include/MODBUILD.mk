#
# Copyright (c) 2008 by Cisco Systems, Inc.
# All Rights Reserved.
#

##########
# MODBUILD.mk
##########

include ../../vqe.mk
include ../../environ.mk

COMMON_HEADERS 	= 		tree.h		\

KERNELPORT_HEADERS	=	

include $(KMODOBJBASE)/kmodrules.mk
