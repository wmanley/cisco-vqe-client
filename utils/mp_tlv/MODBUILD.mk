#
# Copyright (c) 2008 by Cisco Systems, Inc.
# All Rights Reserved.
#

##########
# MODBUILD.mk
##########

include ../../vqe.mk
include ../../environ.mk

COMMON_SOURCES 		=			\
				mp_tlv.c	\
				mp_tlv_decode.c	\


COMMON_OBJECTS 		=	$(patsubst %.c, .$(SRCMODULE)/%.o, $(COMMON_SOURCES))


KERNELPORT_OBJECTS	=	$(patsubst kmod/%.kmod.c, .$(SRCMODULE)/%.o, $(KERNELPORT_SOURCES))


KMODOBJECTS		=	$(COMMON_OBJECTS) $(KERNELPORT_OBJECTS)


KERNELPORT_HEADERS	=	


include $(KMODOBJBASE)/kmodrules.mk

