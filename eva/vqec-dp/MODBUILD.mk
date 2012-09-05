#
# Copyright (c) 2008-2009 by Cisco Systems, Inc.
# All Rights Reserved.
#

##########
# MODBUILD.mk
##########

include ../../vqe.mk
include ../../environ.mk

COMMON_SOURCES 		=	vqec_dp_tlm.c 		\
				vqec_dp_graph.c		\
				vqec_dp_io_stream.c	\
				vqec_dp_syslog.c	\
				vqec_dp_debug_utils.c	\
				vqec_dp_debug.c		\

TEST_READER_SOURCES	=	kmod/test_vqec_reader.kmod.c	\

KERNELPORT_SOURCES	=	kmod/vqec_dp_utils.kmod.c	\
				kmod/vqec_dp_main.kmod.c	\
				kmod/vqec_dp_rpc_server.kmod.c	\
				kmod/vqec_ifclient_read.kmod.c	\

COMMON_OBJECTS 		=	$(patsubst %.c, .$(SRCMODULE)/%.o, $(COMMON_SOURCES))


KERNELPORT_OBJECTS	=	$(patsubst kmod/%.kmod.c, .$(SRCMODULE)/%.o, $(KERNELPORT_SOURCES))


KMODOBJECTS		=	$(COMMON_OBJECTS) $(KERNELPORT_OBJECTS)


COMMON_HEADERS	=		../vqec_tr135_defs.h	\
				vqec_dp_api_types.h	\
				vqec_dp_api.h		\
				vqec_dp_common.h	\
				vqec_dp_debug_flags.h	\
				vqec_dp_debug.h		\
				vqec_dp_debug_utils.h	\
				vqec_dp_event_counter.h	\
				vqec_dp_graph.h		\
				vqec_dp_io_stream.h	\
				vqec_dp_refcnt.h	\
				vqec_dp_syslog_def.h	\
				vqec_dp_tlm_cnt_decl.h	\
				vqec_dp_tlm.h		\
				vqec_dp_utils.h		\


KERNELPORT_HEADERS	=	

include $(KMODOBJBASE)/kmodrules.mk

_cpsources::
	@for cfile in $(TEST_READER_SOURCES); do                                  \
		echo "[cp] =====> $(KMODOBJBASE)/$(ARCH)-dev$(SRCMODULE)/`echo $$cfile | sed 's/kmod[.\/]//g'`"; \
		cp -f -u $$cfile $(KMODOBJBASE)/$(ARCH)-dev$(SRCMODULE)/`echo $$cfile | sed 's/kmod[.\/]//g'`; \
	done;         
