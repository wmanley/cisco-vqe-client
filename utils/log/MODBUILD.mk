#
# Copyright (c) 2008 by Cisco Systems, Inc.
# All Rights Reserved.
#

##########
# MODBUILD.mk
##########

include ../../vqe.mk
include ../../environ.mk

COMMON_SOURCES 		= 	\
					libdebug.c			\
					vqes_syslog_limit.c		\

KERNELPORT_SOURCES	=	kmod/vqec_syslog_stubs.kmod.c	\


COMMON_OBJECTS 		=	$(patsubst %.c, .$(SRCMODULE)/%.o, $(COMMON_SOURCES))


KERNELPORT_OBJECTS	=	$(patsubst kmod/%.kmod.c, .$(SRCMODULE)/%.o, $(KERNELPORT_SOURCES))


KMODOBJECTS		=	$(COMMON_OBJECTS) $(KERNELPORT_OBJECTS)


COMMON_HEADERS 	= 	


KERNELPORT_HEADERS	=	


include $(KMODOBJBASE)/kmodrules.mk

