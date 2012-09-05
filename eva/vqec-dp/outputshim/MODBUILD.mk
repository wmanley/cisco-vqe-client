#
# Copyright (c) 2008 by Cisco Systems, Inc.
# All Rights Reserved.
#

##########
# MODBUILD.mk
##########

include ../../../vqe.mk
include ../../../environ.mk

COMMON_SOURCES 		=	vqec_dp_output_shim.c		\
				vqec_sink_common.c		\

KERNELPORT_SOURCES	=	kmod/vqec_dp_oshim_read_api.kmod.c	\
				kmod/vqec_sink.kmod.c			\

COMMON_OBJECTS 		=	$(patsubst %.c, .$(SRCMODULE)/%.o, $(COMMON_SOURCES))


KERNELPORT_OBJECTS	=	$(patsubst kmod/%.kmod.c, .$(SRCMODULE)/%.o, $(KERNELPORT_SOURCES))


KMODOBJECTS		=	$(COMMON_OBJECTS) $(KERNELPORT_OBJECTS)


COMMON_HEADERS 		= 	vqec_dp_output_shim_api.h	\
				vqec_sink_api.h			\
				vqec_dp_output_shim_private.h	\
				vqec_dp_oshim_read_api.h	\


KERNELPORT_HEADERS	=	kmod/vqec_sink.kmod.h		\

include $(KMODOBJBASE)/kmodrules.mk

