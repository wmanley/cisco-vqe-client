#
# Copyright (c) 2008 by Cisco Systems, Inc.
# All rights reserved.
#

include $(ROOT)/eva/vqec-dp/vqec-dp-make.mk

#
# Location of the channel dataplane library.
#
vqec-dp-channel-lib 	 	= $(LIBDIR)/libvqec-dp-channel.a

#
# Keep make from deleting intermediate files.
#
sources: $(vqec-dp-channel-obj)


CFLAGS += $(INCLUDES)

vqec-dp-channel-src = 				\
	$(SRCDIR)/vqec_dp_rtp_receiver.c	\
	$(SRCDIR)/vqec_dp_rtp_input_stream.c	\
	$(SRCDIR)/vqec_pcm.c			\
	$(SRCDIR)/vqec_oscheduler.c		\
	$(SRCDIR)/vqec_nll.c			\
	$(SRCDIR)/vqec_fec.c			\
	$(SRCDIR)/vqec_dpchan.c			\
	$(SRCDIR)/vqec_log.c			\
	$(SRCDIR)/vqec_dp_sm.c		\


vqec-dp-channel-obj = 			\
	$(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(vqec-dp-channel-src:.c=.o)) 

vqec-dp-channel-members = 		\
	$(vqec-dp-channel-lib)($(vqec-dp-channel-obj))



all::	libs


#
# Build one library from the combination of these multiple libraries.
#
libs::	$(vqec-dp-channel-lib) 

####
# Build PIC libraries
####
pic-dp-channel-lib = $(patsubst %.a, %_r.o, $(vqec-dp-channel-lib))
$(pic-dp-channel-lib): $(vqec-dp-channel-obj)
	$(LD) -r -o $@ $^

pic_libs:: CFLAGS += -fPIC
pic_libs:: $(pic-dp-channel-lib)


#
# channel library build rule.
#
$(vqec-dp-channel-lib): $(vqec-dp-channel-members)
$(vqec-dp-channel-lib)(%o): $(MODOBJ)/%.o
	$(AR) crv $@ $^
$(vqec-dp-channel-lib)():
	$(AR) crv $(vqec-dp-channel-lib) $^


#
# cleanup
#
clean::
	$(RM) $(vqec-dp-channel-lib)

pic_clean::
	$(RM) $(pic-dp-channel-lib)

# -----------------------------------------------------------------------------
# Unit test related stuff.
# -----------------------------------------------------------------------------

vqec-dp-utest-exe = test_vqec_dp_channel_utest

utrun_build::	$(vqec-dp-utest-exe)
.PHONY:	$(vqec-dp-utest-exe)

#
# Unit-test sources.
#
vqec-dp-channel-utest-src = \
	$(SRCDIR)/test_vqec_utest_dp_channel.c			\
	$(SRCDIR)/test_vqec_utest_dpchan.c				\
	$(SRCDIR)/test_vqec_utest_pcm.c				\
	$(SRCDIR)/test_vqec_utest_nll.c				\
	$(SRCDIR)/test_vqec_utest_fec.c				\
	$(SRCDIR)/test_vqec_utest_dpchan_rtp.c			\
	$(SRCDIR)/../test_dp_interposers.c				\
	$(SRCDIR)/../vqec_debug_cp.c					\
        $(SRCDIR)/test_vqec_utest_dp_sm.c                       \

#
# Unit-test library dependencies.
#
# NOTE: libdebug.a is used below (as opposed to libdebug_vqec.a) ONLY for unit
#       tests; libdebuf_vqec.a should be linked with for all regular builds
vqec-dp-utest-dep-lib-names = 			\
	libidmgr.a				\
	rtp.a					\
	libvqecutils.a				\
	libzonemgr.a 				\
	libdebug.a				\
	libmp_tlv.a				\

vqec-dp-all-lib-paths = \
	$(LIBDIR)/libvqec-dp-inputshim.a	\
	$(LIBDIR)/libvqec-dp-channel.a		\
	$(LIBDIR)/libvqec-dp-outputshim.a	\
        $(LIBDIR)/libvqec-dp-common.a	        \

vqec-dp-utest-dep-libs = \
	$(patsubst %, $(LIBDIR)/%, $(vqec-dp-utest-dep-lib-names)) 	\
        $(ROOT)/add-ons/lib/$(ARCH)/libevent.a				\
        -lpthread                                                       \
        -lm                                                             \
        $(vqec-dp-all-lib-paths)                                        \
	$(patsubst %, $(LIBDIR)/%, $(vqec-dp-utest-dep-lib-names)) 	\

vqec-dp-channel-utest-obj = 		\
	$(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(vqec-dp-channel-utest-src:.c=.o)) 

vqec-dp-utest-libs = $(ROOT)/add-ons/lib/$(ARCH)/libcunit.a $(vqec-dp-utest-dep-libs)

test_vqec_dp_channel_utest: $(MODOBJ)/test_vqec_dp_channel_utest
$(MODOBJ)/test_vqec_dp_channel_utest: $(vqec-dp-channel-utest-obj) $(vqec-dp-utest-libs)
	$(CC) $(LDFLAGS) $(CFLAGS) $^ -o $@ $(vqec-dp-utest-libs)


utrun_libs::	CFLAGS +=	-D_VQEC_DP_UTEST
utrun_build::	CFLAGS +=	-D_VQEC_DP_UTEST
clean-utrun::
	$(RM) $(MODOBJ)/$(vqec-dp-utest-exe)
	$(RM) $(MODOBJ)/../*.o

#
# run unit-tests.
#
utrun::	utrun_libs utrun_build
	@echo "Running dataplane channel unit-tests"
	$(MODOBJ)/$(vqec-dp-utest-exe)
	$(LCOV_DIR)/lcov --capture --directory $(MODOBJ) -base-directory . --output-file $(MODOBJ)/utest.info --test-name utest
	$(LCOV_DIR)/genhtml $(MODOBJ)/utest.info --output-directory $(MODOBJ)/output --title "dp-channel unit tests" --show-details --legend


# -----------------------------------------------------------------------------


#
# Sources.
#
SRC +=	$(vqec-dp-channel-src) $(vqec-dp-channel-utest-src)


#
# always include depend.mk except if target does not need it included
# targets that do not need depend.mk are defined in vam.mk. Copy lines
# below as-is.
#
ifeq ($(include-depend-mk), 1)
include $(ROOT)/depend.mk
endif

