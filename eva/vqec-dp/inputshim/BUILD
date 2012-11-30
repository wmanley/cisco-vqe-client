#
# Copyright (c) 2008 by Cisco Systems, Inc.
# All rights reserved.
#

include $(ROOT)/eva/vqec-dp/vqec-dp-make.mk

#
# Location of the inputshim dataplane library.
#
vqec-dp-inputshim-lib 	 	= $(LIBDIR)/libvqec-dp-inputshim.a

#
# Keep make from deleting intermediate files.
#
sources: $(vqec-dp-inputshim-obj)


CFLAGS += $(INCLUDES)

vqec-dp-inputshim-src = 				\
	$(SRCDIR)/vqec_dp_input_shim_api.c		\


vqec-dp-inputshim-obj = 			\
	$(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(vqec-dp-inputshim-src:.c=.o)) 

vqec-dp-inputshim-members = 		\
	$(vqec-dp-inputshim-lib)($(vqec-dp-inputshim-obj))



all::	libs


#
# Build one library from the combination of these multiple libraries.
#
libs::	$(vqec-dp-inputshim-lib) 

#
# inputshim library build rule.
#
$(vqec-dp-inputshim-lib): $(vqec-dp-inputshim-members)
$(vqec-dp-inputshim-lib)(%o): $(MODOBJ)/%.o
	$(AR) crv $@ $^
$(vqec-dp-inputshim-lib)():
	$(AR) crv $(vqec-dp-inputshim-lib) $^

####
# Build PIC libraries
####
pic-dp-inputshim-lib = $(patsubst %.a, %_r.o, $(vqec-dp-inputshim-lib))
$(pic-dp-inputshim-lib): $(vqec-dp-inputshim-obj)
	$(LD) -r -o $@ $^

pic_libs:: CFLAGS += -fPIC
pic_libs:: $(pic-dp-inputshim-lib)


#
# cleanup
#
clean::
	$(RM) $(vqec-dp-inputshim-lib)

pic_clean:: 
	$(RM) $(pic-dp-inputshim-lib)


# -----------------------------------------------------------------------------
# Unit test related stuff.
# -----------------------------------------------------------------------------

vqec-dp-utest-exe = test_vqec_dp_inputshim_utest

utrun_build::	$(vqec-dp-utest-exe)
.PHONY:	$(vqec-dp-utest-exe)

#
# Unit-test sources.
#
vqec-dp-inputshim-utest-src = \
	$(SRCDIR)/test_vqec_utest_input_shim_testmain.c		\
	$(SRCDIR)/test_vqec_utest_input_shim_tests.c    	\
        $(SRCDIR)/../test_dp_interposers.c                      \
        $(SRCDIR)/../vqec_dp_debug_utils.c                      \
	$(SRCDIR)/../../vqe_port.c		\
	$(SRCDIR)/../vqec_dp_syslog.c		\
	$(SRCDIR)/../vqec_debug_cp.c		\

#
# Unit-test library dependencies.
#
# NOTE: libdebug.a is used below (as opposed to libdebug_vqec.a) ONLY for unit
#       tests; libdebuf_vqec.a should be linked with for all regular builds
vqec-dp-utest-dep-lib-names = 			\
	libvqec-dp-common.a                     \
	libidmgr.a				\
	rtp.a					\
	libdebug.a				\
	libvqecutils.a				\
	libzonemgr.a				\

vqec-dp-utest-dep-libs = \
	$(patsubst %, $(LIBDIR)/%, $(vqec-dp-utest-dep-lib-names))	\
        $(ROOT)/add-ons/lib/$(ARCH)/libevent.a                          \
	-lpthread							\
	$(vqec-dp-inputshim-lib)					\


vqec-dp-inputshim-utest-obj = 		\
	$(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(vqec-dp-inputshim-utest-src:.c=.o)) 

vqec-dp-utest-libs = $(ROOT)//add-ons/lib/$(ARCH)/libcunit.a $(vqec-dp-utest-dep-libs)

test_vqec_dp_inputshim_utest: $(MODOBJ)/test_vqec_dp_inputshim_utest
$(MODOBJ)/test_vqec_dp_inputshim_utest: $(vqec-dp-inputshim-utest-obj) $(vqec-dp-utest-libs)
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
	@echo "Running dataplane inputshim unit-tests"
	$(MODOBJ)/$(vqec-dp-utest-exe)
	$(LCOV_DIR)/lcov --capture --directory $(MODOBJ) -base-directory . --output-file $(MODOBJ)/utest.info --test-name utest
	$(LCOV_DIR)/genhtml $(MODOBJ)/utest.info --output-directory $(MODOBJ)/output --title "dp-inputshim unit tests" --show-details --legend


# -----------------------------------------------------------------------------

#
# Sources.
#
SRC +=	$(vqec-dp-inputshim-src) $(vqec-dp-inputshim-utest-src)


#
# always include depend.mk except if target does not need it included
# targets that do not need depend.mk are defined in vam.mk. Copy lines
# below as-is.
#
ifeq ($(include-depend-mk), 1)
include $(ROOT)/depend.mk
endif

