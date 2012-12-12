#
# Copyright (c) 2006-2010 by Cisco Systems, Inc.
# All Rights Reserved.
#

include ../vqe.mk
include ../environ.mk

COMMON_SUBDIRS = \
        utils \
        utils/log \
	utils/mp_tlv \
	rtp \
	sdp \
	cfg \
	stun


VQEC_SUBDIRS = \
	eva \
	vqecutils \
	stunclient \
	rtspclient \
	eva/vqec-dp

VQEC_VOD_SUBDIR = vqec_vod

ifeq ($(HAVE_CISCO_RTSP_LIBRARY), 1)
  VQEC_SUBDIRS += $(VQEC_VOD_SUBDIR)
endif

ALL_SUBDIRS = \
	$(COMMON_SUBDIRS) \
	$(VQEC_SUBDIRS)


SUBDIRS = \
        $(ALL_SUBDIRS)


TAGSIDSRC = \
        $(SUBDIRS) \
        include

KMODDIR := kmod

.PHONY: $(SUBDIRS)


#####Default target definitions#########

all:: libs build

libs::
	@echo Making libs
	@for dir in $(SUBDIRS); do \
		 echo "===> $$dir"; \
		if ! $(MAKE) -wC $$dir libs; then \
			$(MAKE_K_CHECK) \
		fi \
	done

build::
	@echo Building VQE-C sample application
	@if ! $(MAKE) -wC eva build; then \
		$(MAKE_K_CHECK) \
	fi

TAGS::
	@rm -f TAGS
	@echo Building TAGS
	@for dir in $(TAGSIDSRC); do \
		echo "===> $$dir"; \
		TAGSIDSRCSUBDIR=`find $(ROOT)/$$dir -name "*.[ch]" -exec dirname '{}' \; | sort | uniq`; \
		for subdir in $$TAGSIDSRCSUBDIR; do \
			echo "======> $$subdir"; \
			$(ETAGS) --append $$subdir/*.[ch]; \
		done; \
		echo ""; \
	done

ID::
	@rm -f ID
	@echo Building ID
	@touch IDfileList
	@for dir in $(TAGSIDSRC); do \
		echo "$(ROOT)/$$dir" >> IDfileList; \
	done
	$(MKID) --file-list=IDfileList
	@rm -f IDfileList

kmod::
	@echo Building VQE-C kernel module
	@if ! $(MAKE) -wC $(KMODDIR) all; then \
                $(MAKE_K_CHECK) \
        fi

clean-kmod::
	@if ! $(MAKE) -wC $(KMODDIR) clean; then \
                $(MAKE_K_CHECK) \
        fi

clean::
	@echo Cleaning all build components
	@for dir in $(SUBDIRS); do \
		echo "===> $$dir"; \
		if ! $(MAKE) -wC $$dir clean; then \
			$(MAKE_K_CHECK) \
		fi \
	done

clean_release: clean
	@echo "Cleaning 3rd-party libs"; \
	if ! $(MAKE) -w vqec_clean_packages; then \
		echo "failed 3rd-party clean; these may already be clean"; \
	fi;

########## pic target definitions ################

pic::
	@echo making PIC objects
	@for dir in $(SUBDIRS); do 			\
		 echo "===> $$dir"; 			\
		if ! $(MAKE) -wC $$dir pic_libs; then 	\
			$(MAKE_K_CHECK) 		\
		fi 					\
	done
	@echo making PIC .so
	@for dir in $(SUBDIRS); do 			\
		 echo "===> $$dir"; 			\
		if ! $(MAKE) -wC $$dir pic_build; then 	\
			$(MAKE_K_CHECK) 		\
		fi 					\
	done

clean-pic:: 
	@echo cleaning all build components
	@for dir in $(SUBDIRS); do 			\
		echo "===> $$dir"; 			\
		if ! $(MAKE) -wC $$dir pic_clean; then	\
			$(MAKE_K_CHECK) 		\
		fi 					\
	done


# Build variables for configure, third party and other vqec packaging
CONFTOOL                = ./configure
3RDPARTY_SRC_DIR        = $(ROOT)/3rd-party-src
3RDPARTY_INSTALL_DIR    = $(ROOT)/3rd-party-install
LIBEVENT_SRC_DIR        = $(3RDPARTY_SRC_DIR)/libevent/libevent-1.1a
LIBUPNP_SRC_DIR         = $(3RDPARTY_SRC_DIR)/libupnp/libupnp-1.6.6



########## adding destdir support for install #####

INSTALL_INCDIR := $(DESTDIR)$(INSTALL_INCDIR)
INSTALL_LIBDIR := $(DESTDIR)$(INSTALL_LIBDIR)

########## pic target definitions #################

# these libs must be recompiled from the vam/3rd-party-src
# directory when building with a different toolchain
VQEC_PRECOMPILED_LIBS = \
        libevent.a \
        libupnp.a \

VQEC_PUBLIC_HEADERS = \
        eva/vqec_ifclient.h \
        eva/vqec_ifclient_defs.h \
        eva/vqec_tr135_defs.h \
        vqecutils/vqec_ifclient_read.h \
        vqecutils/vqec_error.h \
        vqecutils/vqec_error_macros.h 

ifeq ($(HAVE_FCC), 1)
 MP_TLV_LIB = libmp_tlv.a
 VQEC_PUBLIC_HEADERS += eva/vqec_ifclient_fcc.h
else
 MP_TLV_LIB =
endif

ifeq ($(HAVE_STUN), 1)
 STUN_LIB = stun.a \
            stunclient.a
else
 ifeq ($(HAVE_UPNP), 1)
  STUN_LIB = stunclient.a
 else
  STUN_LIB =
 endif
endif

ifeq ($(HAVE_CISCO_RTSP_LIBRARY), 1)
 CISCO_RTSP_LIB = libvqec_vod.a
 VQEC_PUBLIC_HEADERS += vqec_vod/vqec_vod_ifclient.h
 VQEC_PUBLIC_HEADERS += vqec_vod/vqec_vod_ifclient_defs.h
else
 CISCO_RTSP_LIB =
endif


#
# must use libdebug_vqec.a
#
VQEC_REQUIRED_LIBMODS = \
        librtspclient.a \
        libcfg.a \
        libsdp.a \
        libidmgr.a \
        rtp.a \
        libdebug_vqec.a \
        libzonemgr.a \
        libvqec.a \
        libvqecutils.a \
        libvqec-dp-channel.a \
        libvqec-dp-common.a \
        libvqec-dp-inputshim.a \
        libvqec-dp-outputshim.a \
        $(MP_TLV_LIB) \
        $(STUN_LIB) \
	$(CISCO_RTSP_LIB) \

#
# [append a root path for installation of files in targets.]
#
VQEC_LIBS_FOR_INSTALL = \
        $(patsubst %, $(ROOT)/vqec_lib/%, $(VQEC_REQUIRED_LIBMODS)) \
        $(patsubst %, $(ROOT)/add-ons/lib/$(ARCH)/%, $(VQEC_PRECOMPILED_LIBS))

VQEC_UBER_LIB_FOR_INSTALL := $(ROOT)/vqec_lib/libvqecuber.a

VQEC_SO_LIBS_FOR_INSTALL = \
        $(patsubst %.a, $(ROOT)/add-ons/lib/$(ARCH)/%, $(VQEC_PRECOMPILED_LIBS))

VQEC_HEADERS_FOR_INSTALL = $(patsubst %, $(ROOT)/%, $(VQEC_PUBLIC_HEADERS))

#
# VQEC's auto-configured 3rd-party lib source-dirs.
#
VQEC_AC_LIBDIRS =               \
        $(LIBEVENT_SRC_DIR)     \

#
# VQEC's 3rd-party library names.
#
VQEC_3RDPARTY_LIBS =                                            \
        $(LIBEVENT_SRC_DIR)/.libs/libevent.a                    \
        $(LIBUPNP_SRC_DIR)/libupnp.a

# VQEC's 3rd-party dynamic libs.
#
VQEC_3RDPARTY_SO_LIBS =                                            \
        $(patsubst %.a, %, $(VQEC_3RDPARTY_LIBS))

#
#
# This will come from autoconf if we can use config.guess & config.sub;
#
ifeq ($(OBJECT_4K), 1)
 AUTOCONF_TARGET = $(ARCH)
else
  ifeq ($(OBJECT_SH4), 1)
   AUTOCONF_TARGET = $(ARCH)
  else
   AUTOCONF_TARGET =
  endif
endif

.PHONY: vqec_config_packages vqec_build_packages vqec_clean_packages

vqec_config_packages:
	@for dir in $(VQEC_AC_LIBDIRS); do \
		if ! cd $$dir; then exit 1; \
		else \
			echo "===> entering $$dir"; \
			cd $$dir; \
			if ! $(CONFTOOL) CC=$(CC) CXX=$(CXX) AR=$(AR) LD=$(LD) \
				--host=$(AUTOCONF_TARGET) \
				--prefix=$(3RDPARTY_INSTALL_DIR); \
				then exit 1; fi; \
		fi \
	done

vqec_build_packages:
	@mkdir -p $(ROOT)/add-ons/lib/$(ARCH)
	@for dir in $(VQEC_AC_LIBDIRS); do \
		if ! cd $$dir; then exit 1; \
		else \
			echo "===> entering $$dir"; \
			cd $$dir; \
			if ! $(MAKE); then exit 1; fi; \
		fi \
	done
	if ! cd $(LIBUPNP_SRC_DIR); then exit 1; \
	else \
		cd $(LIBUPNP_SRC_DIR); \
		if ! $(MAKE); then exit 1; fi; \
	fi
	@for lib in $(VQEC_3RDPARTY_LIBS); do \
		if ! cp -f $$lib $(ROOT)/add-ons/lib/$(ARCH); \
		then exit 1; \
		else echo "copied $$lib to $(ROOT)/add-ons/lib/$(ARCH)"; \
		fi \
	done
	@for lib in $(VQEC_3RDPARTY_SO_LIBS); do \
		if ! cp -f -P $$lib*.so.* $(ROOT)/add-ons/lib/$(ARCH); \
		then exit 1; \
		else echo "copied $$lib to $(ROOT)/add-ons/lib/$(ARCH)"; \
		cp -f -P $$lib*.so $(ROOT)/add-ons/lib/$(ARCH); \
		fi \
	done

vqec_clean_packages:
	@for dir in $(VQEC_AC_LIBDIRS); do \
		if ! cd $$dir; then exit 1; \
		else \
			echo "===> entering $$dir"; \
			cd $$dir; \
			$(MAKE) clean; \
		fi \
	done

	if ! cd $(LIBUPNP_SRC_DIR); then exit 1; \
	else \
		cd $(LIBUPNP_SRC_DIR); \
		$(MAKE) clean; \
	fi


vqec_uber_archive:
	@mkdir -p uberTmp; \
	cd uberTmp; \
	for file in $(VQEC_LIBS_FOR_INSTALL); do \
		ar x $$file; \
	done; \
	rm -f $(VQEC_UBER_LIB_FOR_INSTALL); \
	ar rc $(VQEC_UBER_LIB_FOR_INSTALL) *.o

vqec_install: vqec_uber_archive
	@echo DESTDIR=$(DESTDIR)
	@echo INCDIR=$(INSTALL_INCDIR)
	@echo LIBDIR=$(INSTALL_LIBDIR)

	@for file in $(VQEC_LIBS_FOR_INSTALL) $(VQEC_UBER_LIB_FOR_INSTALL); do \
	if [ ! -e $$file ]; then \
		echo "not copying $$file: file not found!"; \
		else \
			if [ ! -d $(INSTALL_LIBDIR) ]; then mkdir -p $(INSTALL_LIBDIR); fi; \
			cp $$file $(INSTALL_LIBDIR); \
			echo "copied $$file to $(INSTALL_LIBDIR)"; \
			$(STRIP) -g $(INSTALL_LIBDIR)/`basename $$file`; \
			echo "Stripped debug symbols from $(INSTALL_LIBDIR)/`basename $$file`"; \
		fi \
	done
	@for file in $(VQEC_SO_LIBS_FOR_INSTALL); do \
	if [ ! -e $$file*.so ]; then \
		echo "not copying $$file*.so: file not found!"; \
		else \
			if [ ! -d $(INSTALL_LIBDIR) ]; then mkdir -p $(INSTALL_LIBDIR); fi; \
			cp -f -P $$file*.so.* $(INSTALL_LIBDIR); \
			echo "copied $$file.so to $(INSTALL_LIBDIR)"; \
			$(STRIP) -g $(INSTALL_LIBDIR)/`basename $$file*.so.*.*.*`; \
			echo "Stripped debug symbols from $(INSTALL_LIBDIR)/`basename $$file.so`"; \
			cp -f -P $$file.so $(INSTALL_LIBDIR); \
		fi \
	done
	@if [ -e $(ROOT)/vqec_lib/pic/libvqec_r.*.so ]; then \
		cp -f -P $(ROOT)/vqec_lib/pic/libvqec_r.*.so $(INSTALL_LIBDIR); \
		echo "copied $(ROOT)/vqec_lib/pic/libvqec_r.*.so to $(INSTALL_LIBDIR)"; \
		$(STRIP) -g $(INSTALL_LIBDIR)/libvqec_r.*.so; \
		echo "Stripped debug symbols from $(INSTALL_LIBDIR)/libvqec_r.*.so"; \
                cd $(INSTALL_LIBDIR); \
		ln -sf libvqec_r.*.so $(INSTALL_LIBDIR)/libvqec_r.so; \
	fi
	@for file in $(VQEC_HEADERS_FOR_INSTALL); do \
		if [ ! -e $$file ]; then \
		echo "not copying $$file: file not found!"; \
		else \
			if [ ! -d $(INSTALL_INCDIR) ]; then mkdir -p $(INSTALL_INCDIR); fi; \
			cp $$file $(INSTALL_INCDIR); \
			echo "copied $$file to $(INSTALL_INCDIR)"; \
		fi \
	done

# DO NOT DELETE

