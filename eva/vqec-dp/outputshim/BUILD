#
# Copyright (c) 2008 by Cisco Systems, Inc.
# All rights reserved.
#

include $(ROOT)/eva/vqec-dp/vqec-dp-make.mk

#
# Location of the outputshim dataplane library.
#
vqec-dp-outputshim-lib 	 	= $(LIBDIR)/libvqec-dp-outputshim.a

#
# Keep make from deleting intermediate files.
#
sources: $(vqec-dp-outputshim-obj)


CFLAGS += $(INCLUDES)

vqec-dp-outputshim-src = 				\
	$(SRCDIR)/vqec_dp_output_shim.c			\
	$(SRCDIR)/vqec_dp_oshim_read_api.c		\
	$(SRCDIR)/vqec_sink.c				\
	$(SRCDIR)/vqec_sink_common.c			\


vqec-dp-outputshim-obj = 			\
	$(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(vqec-dp-outputshim-src:.c=.o)) 

vqec-dp-outputshim-members = 		\
	$(vqec-dp-outputshim-lib)($(vqec-dp-outputshim-obj))



all::	libs


#
# Build one library from the combination of these multiple libraries.
#
libs::	$(vqec-dp-outputshim-lib) 

####
# Build PIC libraries
####
pic-dp-outputshim-lib = $(patsubst %.a, %_r.o, $(vqec-dp-outputshim-lib))
$(pic-dp-outputshim-lib): $(vqec-dp-outputshim-obj)
	$(LD) -r -o $@ $^

pic_libs:: CFLAGS += -fPIC
pic_libs:: $(pic-dp-outputshim-lib)


#
# outputshim library build rule.
#
$(vqec-dp-outputshim-lib): $(vqec-dp-outputshim-members)
$(vqec-dp-outputshim-lib)(%o): $(MODOBJ)/%.o
	$(AR) crv $@ $^
$(vqec-dp-outputshim-lib)():
	$(AR) crv $(vqec-dp-outputshim-lib) $^


#
# cleanup
#
clean::
	$(RM) $(vqec-dp-outputshim-lib)

pic_clean:: 
	$(RM) $(pic-dp-outputshim-lib)

# -----------------------------------------------------------------------------
# Unit test related stuff.
# -----------------------------------------------------------------------------

vqec-dp-utest-exe = test_vqec_dp_outputshim_utest

utrun_build::	$(vqec-dp-utest-exe)
.PHONY:	$(vqec-dp-utest-exe)

#
# Unit-test sources.
#
vqec-dp-outputshim-utest-src = 					\
	$(SRCDIR)/test_vqec_utest_dp_outputshim.c		\
	$(SRCDIR)/test_vqec_utest_dp_outputshim_api.c           \
	$(SRCDIR)/../test_dp_interposers.c                      \
	$(SRCDIR)/../../vqe_port.c		\
	$(SRCDIR)/../vqec_dp_syslog.c		\
	$(SRCDIR)/../vqec_debug_cp.c		\

#
# Unit-test library dependencies.
#
# NOTE: libdebug.a is used below (as opposed to libdebug_vqec.a) ONLY for unit
#       tests; libdebuf_vqec.a should be linked with for all regular builds
vqec-dp-utest-dep-lib-names =                   \
        libidmgr.a                              \
        rtp.a                                   \
        libdebug.a				\
        libvqecutils.a                          \
        libzonemgr.a                            \
	libvqec-dp-common.a                     \
	
vqec-dp-utest-dep-libs = \
        $(patsubst %, $(LIBDIR)/%, $(vqec-dp-utest-dep-lib-names)) 	\
        $(ROOT)/add-ons/lib/$(ARCH)/libevent.a				\
        -lpthread                                                       \
        $(vqec-dp-outputshim-lib)                                       \

vqec-dp-outputshim-utest-obj = 		\
	$(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(vqec-dp-outputshim-utest-src:.c=.o)) 

vqec-dp-utest-libs = $(ROOT)/add-ons/lib/$(ARCH)/libcunit.a $(vqec-dp-utest-dep-libs)

test_vqec_dp_outputshim_utest: $(MODOBJ)/test_vqec_dp_outputshim_utest
$(MODOBJ)/test_vqec_dp_outputshim_utest: $(vqec-dp-outputshim-utest-obj) $(vqec-dp-utest-libs)
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
	@echo "Running dataplane outputshim unit-tests"
	$(MODOBJ)/$(vqec-dp-utest-exe)
	$(LCOV_DIR)/lcov --capture --directory $(MODOBJ) -base-directory . --output-file $(MODOBJ)/utest.info --test-name utest
	$(LCOV_DIR)/genhtml $(MODOBJ)/utest.info --output-directory $(MODOBJ)/output --title "dp-outputshim unit tests" --show-details --legend


# -----------------------------------------------------------------------------


#
# Sources.
#
SRC +=	$(vqec-dp-outputshim-src) $(vqec-dp-outputshim-utest-src)


#
# always include depend.mk except if target does not need it included
# targets that do not need depend.mk are defined in vam.mk. Copy lines
# below as-is.
#
ifeq ($(include-depend-mk), 1)
include $(ROOT)/depend.mk
endif

