#
# Copyright (c) 2008 by Cisco Systems, Inc.
# All Rights Reserved.
#

##########
# MODBUILD.mk
##########

include ../vqe.mk
include ../environ.mk

COMMON_SOURCES 		= 			\
				strlcpy.c	\
				strlcat.c	\
				id_manager.c	\
				vqe_bitmap.c	\
				vam_hist.c	\


KERNELPORT_SOURCES	=	kmod/zone_mgr.kmod.c	\
				kmod/vam_util.kmod.c	\


COMMON_OBJECTS 		=	$(patsubst %.c, .$(SRCMODULE)/%.o, $(COMMON_SOURCES))


KERNELPORT_OBJECTS	=	$(patsubst kmod/%.kmod.c, .$(SRCMODULE)/%.o, $(KERNELPORT_SOURCES))


KMODOBJECTS		=	$(COMMON_OBJECTS) $(KERNELPORT_OBJECTS)


COMMON_HEADERS 		= 	id_manager_private.h 	\


KERNELPORT_HEADERS	=	


include $(KMODOBJBASE)/kmodrules.mk

