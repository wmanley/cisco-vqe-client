#
# Copyright (c) 2008 by Cisco Systems, Inc.
# All Rights Reserved.
#

##########
# MODBUILD.mk
##########

include ../../../vqe.mk
include ../../../environ.mk

COMMON_SOURCES 		=	vqec_dp_input_shim_api.c	\


KERNELPORT_SOURCES	=	


COMMON_OBJECTS 		=	$(patsubst %.c, .$(SRCMODULE)/%.o, $(COMMON_SOURCES))


KERNELPORT_OBJECTS	=	$(patsubst kmod/%.kmod.c, .$(SRCMODULE)/%.o, $(KERNELPORT_SOURCES))


KMODOBJECTS		=	$(COMMON_OBJECTS) $(KERNELPORT_OBJECTS)


COMMON_HEADERS 		= 	vqec_dp_input_shim_api.h		\
				vqec_dp_input_shim_private.h		\


KERNELPORT_HEADERS	=	


include $(KMODOBJBASE)/kmodrules.mk

