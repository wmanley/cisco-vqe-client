#
# Copyright (c) 2008 by Cisco Systems, Inc.
# All Rights Reserved.
#

##########
# MODBUILD.mk
##########

include ../vqe.mk
include ../environ.mk

COMMON_SOURCES 		=				\
				vqec_pak_seq.c 		\
				vqec_pak.c		\


KERNELPORT_SOURCES	=					\
				kmod/vqec_debug.kmod.c		\
				kmod/vqec_event.kmod.c		\
				kmod/vqec_recv_socket.kmod.c	\


COMMON_OBJECTS 		=	$(patsubst %.c, .$(SRCMODULE)/%.o, $(COMMON_SOURCES))


KERNELPORT_OBJECTS	=	$(patsubst kmod/%.kmod.c, .$(SRCMODULE)/%.o, $(KERNELPORT_SOURCES))


KMODOBJECTS		=	$(COMMON_OBJECTS) $(KERNELPORT_OBJECTS)


COMMON_HEADERS 		= 	vqec_pak_seq.h		\
				vqec_event.h		\
				vqec_pak_hdr.h		\
				vqec_seq_num.h		\
				vqec_recv_socket.h	\
				vqec_debug.h		\
				vqec_error.h		\
				vqec_error_macros.h	\
				vqec_debug_flags.h	\
				vqec_syslog_def.h	\
				vqec_pak_hdr.h		\
				vqec_ifclient_read.h	\

KERNELPORT_HEADERS	=	kmod/vqec_lock.kmod.h	\
				kmod/vqec_pak.kmod.h	\
				kmod/vqec_recv_socket_macros.kmod.h	\



include $(KMODOBJBASE)/kmodrules.mk

