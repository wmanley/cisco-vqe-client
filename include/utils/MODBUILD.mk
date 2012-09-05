#
# Copyright (c) 2008 by Cisco Systems, Inc.
# All Rights Reserved.
#

##########
# MODBUILD.mk
##########

include ../../vqe.mk
include ../../environ.mk

COMMON_HEADERS 	= 		id_manager.h 	\
				queue_plus.h	\
				strl.h		\
				vqe_bitmap.h	\
				zone_mgr.h	\
				vam_hist.h	\
				mp_tlv.h	\
				mp_mpeg.h	\
				mp_tlv_decode.h\

KERNELPORT_HEADERS	=	kmod/vam_types.kmod.h	\
				kmod/vam_util.kmod.h	\
				kmod/vqe_port_macros.kmod.h	\
				kmod/vam_time.kmod.h	\

include $(KMODOBJBASE)/kmodrules.mk
