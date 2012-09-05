#
# Copyright (c) 2006-2010 by Cisco Systems, Inc.
# All rights reserved.
#

#
# This file contains variables used by the VQE-C Makefiles:
#   Makefile
#   sample/Makefile
#   utest/Makefile
#

#local include paths go here
INCLUDE_PATHS += $(SRCDIR) 	\
	$(MODOBJ)		\
	$(ROOT)			\
	$(ROOT)/eva		\
	$(ROOT)/eva/vqec-dp	\
	$(ROOT)/eva/vqec-dp/inputshim	\
	$(ROOT)/eva/vqec-dp/channel	\
	$(ROOT)/eva/vqec-dp/outputshim	\
	$(ROOT)/vqecutils	\
	$(ROOT)/vqeciplm	\
	$(ROOT)/cm 		\
	$(ROOT)/utils 		\
	$(ROOT)/cli 		\
        $(ROOT)/rtp             \
	$(ROOT)/stun 		\
	$(ROOT)/stunclient       \
	$(ROOT)/include 	\
	$(ROOT)/include/utils 	\
	$(ROOT)/include/log	\
        $(ROOT)/add-ons/include \
	$(ROOT)/cfg		\
	$(ROOT)/elib		\
	$(ROOT)/rtspclient      \
        $(ROOT)/vqec_vod

INCLUDES = $(addprefix -I, $(INCLUDE_PATHS))

LDFLAGS = -L$(LIBDIR) -L$(ROOT)/add-ons/lib/$(ARCH)

#
# Sources (categorized)
#
VQEC_SRC_ELOG =				\
	$(SRCDIR)/vqec_elog.c			\

#
# Internal dependencies (libs built in our domain)
#
VQEC_CP_DEP_LIBS =			\
	librtspclient.a 		\
	libcfg.a 			\
	libsdp.a 			\
	libidmgr.a			\
	$(VQEC_DEP_STUN)		\
	stun.a				\
	rtp.a				\
	libdebug_vqec.a			\
	libvqecutils.a			\
	libmp_tlv.a			\
	libzonemgr.a 			\
	$(VQEC_DEP_CISCO_RTSP_LIB)	\

VQEC_DP_DEP_LIBS =			\
	libvqec-dp-outputshim.a         \
	libvqec-dp-inputshim.a          \
	libvqec-dp-channel.a            \
	libvqec-dp-common.a             \

VQEC_DEP_VQE_LIBS =			\
	$(VQEC_DP_DEP_LIBS)		\
	$(VQEC_CP_DEP_LIBS)		\

VQEC_DEP_CP_VQE_LIBS =			\
	$(VQEC_CP_DEP_LIBS)		\

ifeq ($(HAVE_IPLM), 1)
	VQEC_DEP_VQE_LIBS += libvqeciplm.a
endif


VQEC_DEP_ADD_ON_LIBS =			\
	libevent.a			\
	libupnp.a                       \

VQEC_DEP_VQE = 	$(patsubst %, $(LIBDIR)/%, $(VQEC_DEP_VQE_LIBS)) \
		$(patsubst %, $(ROOT)/add-ons/lib/$(ARCH)/%, $(VQEC_DEP_ADD_ON_LIBS))

VQEC_DEP_CP_VQE = 	$(patsubst %, $(LIBDIR)/%, $(VQEC_DEP_CP_VQE_LIBS)) 		\
			$(patsubst %, $(ROOT)/add-ons/lib/$(ARCH)/%, $(VQEC_DEP_ADD_ON_LIBS))

VQEC_DEP_VQE_SO = 	$(patsubst %, $(LIBDIR)/%, $(VQEC_DEP_VQE_LIBS)) 

#
# External dependencies (libs from environment)
#
VQEC_DEP_EXTERNAL =			\
	-lpthread 			\
	-lcrypt				\
	-lresolv			\
	-lm

VQEC_DEP_EXTERNAL_SO =		\
	$(VQEC_DEP_EXTERNAL)	\
	-levent			\

ifeq ($(HAVE_UPNP), 1)
	VQEC_DEP_EXTERNAL_SO += -lupnp
endif

ifeq ($(HAVE_OPENSSL), 1)
	VQEC_DEP_EXTERNAL += -lssl
endif

#
# Optional dependencies
#
ifeq ($(HAVE_ELOG), 1)
 ELOG_OBJ	     	= $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(VQEC_SRC_ELOG:.c=.o))
 VQEC_DEP_ELOG =			\
	-lrt				\
	$(LIBDIR)/elib.a
else
 ELOG_OBJ	=
 VQEC_DEP_ELOG	=
endif

ifeq ($(HAVE_SYSLOG), 1)
 CFLAGS += -DSYS_HAS_SYSLOG
endif

ifeq ($(HAVE_STUN), 1)
 VQEC_DEP_STUN = \
	stunclient.a
else
 ifeq ($(HAVE_UPNP), 1)
  VQEC_DEP_STUN = \
     stunclient.a
 else
  VQEC_DEP_STUN = 
 endif
endif

ifeq ($(HAVE_CISCO_RTSP_LIBRARY), 1)
 VQEC_DEP_CISCO_RTSP_LIB = \
	libvqec_vod.a
endif

#
# Locations of compiled libraries for VQE-C
#
VQEC_LIB 	 	= $(LIBDIR)/libvqec.a
VQEC_LIB_KMOD 	 = $(LIBDIR)/libvqec_kmod.a

#
# Relocatable objects.
#
VQEC_DEP_LIBS_R	= 	$(patsubst %.a, $(LIBDIR)/%_r.o, $(VQEC_DEP_VQE_LIBS))
VQEC_DEP_LIBS_KMOD_R	= 	$(patsubst %.a, $(LIBDIR)/%_r.o, $(VQEC_DEP_CP_VQE_LIBS))
VQEC_DEP_ADDONS_R	=	$(patsubst %, $(ROOT)/add-ons/lib/$(ARCH)/%, $(VQEC_DEP_ADD_ON_LIBS))
VQEC_DEP_CP_VQE_R	= 	$(patsubst %.a, %_r.o,  $(VQEC_DEP_CP_VQE))
VQEC_DEP_ELOG_R	= 	$(patsubst %.a, %_r.o,  $(VQEC_DEP_ELOG))
VQEC_PIC_LIB		= 	$(patsubst %.a, %_r.o,  $(VQEC_LIB))
VQEC_PIC_LIB_KMOD	= 	$(patsubst %.a, %_r.o,  $(VQEC_LIB_KMOD))

#
# Keep make from deleting intermediate files.
#
sources: $(VQEC_LIB_OBJ) $(ELOG_OBJ)

