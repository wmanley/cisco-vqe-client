#
# Copyright (c) 2006-2009 by Cisco Systems, Inc.
# All rights reserved.
#

include $(ROOT)/eva/vqecmake.mk

LIB_INCLUDES = $(INCLUDES) -I$(ROOT)/eva/vqec-dp -I$(ROOT)/eva/vqec-dp/outputshim -I$(ROOT)/eva/vqec-dp/channel
CFLAGS +=	 $(LIB_INCLUDES)

VQEC_SRC =                                        \
        $(SRCDIR)/vqec_igmp.c                     \
        $(SRCDIR)/vqec_drop.c                     \
        $(SRCDIR)/vqec_sm.c                       \
        $(SRCDIR)/vqec_rtp.c                      \
        $(SRCDIR)/vqec_tuner.c                    \
        $(SRCDIR)/vqec_channel.c                  \
        $(SRCDIR)/vqec_channel_cli.c              \
        $(SRCDIR)/vqec_url.c                      \
        $(SRCDIR)/vqec_ifclient.c                 \
        $(SRCDIR)/vqec_syscfg.c                   \
        $(SRCDIR)/vqe_port.c                      \
        $(SRCDIR)/vqec_cli_register.c             \
        $(SRCDIR)/rtp_repair_recv.c               \
        $(SRCDIR)/rtp_era_recv.c                  \
        $(SRCDIR)/vqec_stream_output_thread_mgr.c \
        $(SRCDIR)/vqec_updater.c                  \
        $(SRCDIR)/vqec_pthread.c                  \
        $(SRCDIR)/vqec_gaptree.c                  \
        $(SRCDIR)/vqec_gap_reporter.c             \
        $(SRCDIR)/vqec_nat_api.c                  \
        $(SRCDIR)/vqec_upcall_event.c	          \
	$(SRCDIR)/vqec_nat_natproto.c		  \
        $(SRCDIR)/vqec_cli_interface.c            \
        $(SRCDIR)/vqec_config_parser.c            \
        $(SRCDIR)/vqec_cli.c                      \

ifeq ($(HAVE_FCC), 1)
 VQEC_SRC += $(SRCDIR)/vqec_ifclient_fcc.c
endif

ifeq ($(HAVE_IPLM), 1)
 VQEC_SRC += $(SRCDIR)/iplm_cli_register.c
endif

VQEC_KMOD_FILES	= 	\
	$(SRCDIR)/vqec_rpc_client.c	\

VQEC_SRC_KMOD	= 		\
	$(VQEC_SRC)			\
	$(VQEC_KMOD_FILES)	\

VQEC_LIB_OBJ 	 	= $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(VQEC_SRC:.c=.o)) $(ELOG_OBJ)
VQEC_LIB_MEMBERS 	= $(VQEC_LIB)($(VQEC_LIB_OBJ))

VQEC_LIB_OBJ_KMOD 	 	= $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(VQEC_SRC_KMOD:.c=.o)) $(ELOG_OBJ)
VQEC_LIB_MEMBERS_KMOD 	= $(VQEC_LIB_KMOD)($(VQEC_LIB_OBJ_KMOD))

all:: 	libs build

utrun:: utrun_libs utrun_build
	@echo "Running VQEC unit tests"; \
	if ! $(MAKE) -wC utest utrun; then \
	 $(MAKE_K_CHECK) \
	fi

#
# The top-level Makefile calls either
#  libs build
# or
#  utrun_libs utrun_build
# vqec.mk defines utrun_build to point to build, but
# we only want to do a regular 'build' when we're NOT building
# unit tests, so we'll keep track of what target
# was actually called here, and act accordingly.
#
BUILDING_UTESTS = 0

utrun_libs:: CFLAGS +=	-D_VQEC_UTEST_INTERPOSERS

utrun_build:: BUILDING_UTESTS = 1

.PHONY: $(utrun_libs)

#
# Libraries built here
#
libs:: make_ver $(VQEC_LIB) $(VQEC_LIB_KMOD) update_ver

$(VQEC_LIB): $(VQEC_LIB_MEMBERS)
$(VQEC_LIB)(%o): $(MODOBJ)/%.o
	$(AR) crv $@ $^
$(VQEC_LIB)():
	$(AR) crv $(VQEC_LIB) $^


$(VQEC_LIB_KMOD): $(VQEC_LIB_MEMBERS_KMOD)
$(VQEC_LIB_KMOD)(%o): $(MODOBJ)/%.o
	$(AR) crv $@ $^
$(VQEC_LIB_KMOD)():
	$(AR) crv $(VQEC_LIB_KMOD) $^

$(VQEC_PIC_LIB): $(VQEC_LIB_OBJ)
	$(LD) -r -o $@ $^

$(VQEC_PIC_LIB_KMOD): $(VQEC_LIB_OBJ_KMOD)
	$(LD) -r -o $@ $^

VQEC_UTEST_SRC = $(SRCDIR)/test_vqec_utest_interposers.c
VQEC_UTEST_OBJ = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(VQEC_UTEST_SRC:.c=.o))
utrun_libs:: $(VQEC_UTEST_OBJ)
	$(AR) crv $(VQEC_LIB) $^

utrun_libs:: $(VQEC_UTEST_OBJ)
	$(AR) crv $(VQEC_LIB_KMOD) $^


pic_libs:: make_ver $(VQEC_PIC_LIB) $(VQEC_PIC_LIB_KMOD) update_ver_pic

pic_build:: 
	@echo "Building .so library for vqec"
	@if ! $(MAKE) -wC sample pic_build; then 	\
	  $(MAKE_K_CHECK) 			\
	fi					\


#
# Applications built here
#
build::
	@if [ $(BUILDING_UTESTS) = 0 ]; then \
	  echo "Building VQE-C sample apps"; \
	  if [ "$(MAKECMDGOALS)" = "" ]; then \
	   if ! $(MAKE) -wC sample build; then \
	     $(MAKE_K_CHECK) \
	   fi \
	  else \
	   if ! $(MAKE) -wC sample $(MAKECMDGOALS); then \
	     $(MAKE_K_CHECK) \
	   fi \
	  fi \
	fi


utrun_build::
	@echo "Building VQE-C sample apps with gcov"; \
	if ! $(MAKE) -wC sample utrun_build; then \
	  $(MAKE_K_CHECK) \
	fi
	@echo "Building VQE-C unit tests"; \
	if ! $(MAKE) -wC utest utrun_build; then \
	  $(MAKE_K_CHECK) \
	fi \

#make_obj_dirs:
#	@mkdir -p $(MODOBJ)/utest
#	@mkdir -p $(MODOBJ)/sample


update_ver: $(VQEC_LIB) $(ELOG_OBJ)
	@cd $(SRCDIR); \
	./update_version.sh $(VER_BUILD_TYPE); \
	echo "updated vqec_version.h"; \
	touch update_ver;

update_ver_pic: $(VQEC_PIC_LIB) $(ELOG_OBJ)
	@cd $(SRCDIR); \
	./update_version.sh $(VER_BUILD_TYPE); \
	echo "updated vqec_version.h"; \
	touch update_ver;

make_ver:
	@cd $(SRCDIR); \
	if [ ! -e vqec_version.h ]; then \
		./update_version.sh $(VER_BUILD_TYPE); \
		echo "created vqec_version.h"; \
	fi;

GENINCLUDES = $(SRCDIR)/vqec_rpc_common.h_rpcgen $(SRCDIR)/vqec_rpc_client.c_rpcgen

$(SRCDIR)/vqec_rpc_common.h_rpcgen: $(SRCDIR)/vqec-dp/vqec_dp_api.h $(SRCDIR)/vqec_rpcgen.pl \
		$(SRCDIR)/vqec-dp/vqec_dp_api_types.h
	$(SRCDIR)/vqec_rpcgen.pl -in $< -common $@ -module vqec;

$(SRCDIR)/vqec_rpc_client.c_rpcgen: $(SRCDIR)/vqec-dp/vqec_dp_api.h $(SRCDIR)/vqec_rpcgen.pl \
		$(SRCDIR)/vqec-dp/vqec_dp_api_types.h
	$(SRCDIR)/vqec_rpcgen.pl -in $< -client $@ -module vqec;

clean::
	$(RM) $(VQEC_LIB)
	$(RM) $(VQEC_LIB_KMOD)
	$(RM) $(SRCDIR)/vqec_version.h
	$(RM) $(SRCDIR)/update_ver
	$(RM) $(SRCDIR)/*.c_rpcgen
	$(RM) $(SRCDIR)/*.h_rpcgen
	@$(MAKE) -wC utest $(MAKECMDGOALS)
	@$(MAKE) -wC sample $(MAKECMDGOALS)

pic_clean:: 
	$(RM) $(VQEC_PIC_LIB)

#
# Sources
#
SRC += 	$(VQEC_SRC) 		\
	$(VQEC_SRC_ELOG) 	\
	$(VQEC_KMOD_FILES)

# always include depend.mk except if target does not need it included
# targets that do not need depend.mk are defined in vam.mk. Copy lines
# below as-is.
ifeq ($(include-depend-mk), 1)
include $(ROOT)/depend.mk
endif

