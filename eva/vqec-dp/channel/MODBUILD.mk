#
# Copyright (c) 2008 by Cisco Systems, Inc.
# All Rights Reserved.
#

##########
# MODBUILD.mk
##########

include ../../../vqe.mk
include ../../../environ.mk

COMMON_SOURCES 		=					\
				vqec_nll.c			\
				vqec_log.c			\
				vqec_dp_rtp_receiver.c		\
				vqec_dp_rtp_input_stream.c 	\
				vqec_oscheduler.c		\
				vqec_pcm.c			\
				vqec_fec.c			\
				vqec_dpchan.c			\
				vqec_dp_sm.c			\

KERNELPORT_SOURCES	=	


COMMON_OBJECTS 		=	$(patsubst %.c, .$(SRCMODULE)/%.o, $(COMMON_SOURCES))


KERNELPORT_OBJECTS	=	$(patsubst kmod/%.kmod.c, .$(SRCMODULE)/%.o, $(KERNELPORT_SOURCES))


KMODOBJECTS		=	$(COMMON_OBJECTS) $(KERNELPORT_OBJECTS)


COMMON_HEADERS 	= 		vqec_dpchan_api.h		\
				vqec_dp_rtp_input_stream.h	\
				vqec_dp_rtp_receiver.h		\
				vqec_dp_sm.h			\
				vqec_fec.h			\
				vqec_log.h			\
				vqec_nll.h			\
				vqec_oscheduler.h		\
				vqec_pcm.h			\
				vqec_dpchan.h			\


KERNELPORT_HEADERS	=	


include $(KMODOBJBASE)/kmodrules.mk

