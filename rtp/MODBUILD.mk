#
# Copyright (c) 2008-2009 by Cisco Systems, Inc.
# All Rights Reserved.
#

##########
# MODBUILD.mk
##########

include ../vqe.mk
include ../environ.mk

COMMON_SOURCES 		=	\
					rtp_header.c 	\
					rtcp_xr.c	\

KERNELPORT_SOURCES	=	


COMMON_OBJECTS 		=	$(patsubst %.c, .$(SRCMODULE)/%.o, $(COMMON_SOURCES))


KERNELPORT_OBJECTS	=	$(patsubst kmod/%.kmod.c, .$(SRCMODULE)/%.o, $(KERNELPORT_SOURCES))


KMODOBJECTS		=	$(COMMON_OBJECTS) $(KERNELPORT_OBJECTS)


COMMON_HEADERS 		= 	rtp.h		\
				rtp_types.h	\
				rtp_header.h	\
                                fec_fast_type.h \
				rtcp_xr.h       \
				rtcp.h          \

KERNELPORT_HEADERS	=	


include $(KMODOBJBASE)/kmodrules.mk

