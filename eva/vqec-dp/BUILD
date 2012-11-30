#
# Copyright (c) 2008 by Cisco Systems, Inc.
# All rights reserved.
#

include $(ROOT)/eva/vqec-dp/vqec-dp-make.mk

#
# Location of the common dataplane library.
#
vqec-dp-common-lib 	 	= $(LIBDIR)/libvqec-dp-common.a

#
# Keep make from deleting intermediate files.
#
sources: $(vqec-dp-common-obj) $(test-vqec-dp-obj)


CFLAGS += $(INCLUDES)

vqec-dp-common-src =                            \
	$(SRCDIR)/vqec_dp_graph.c               \
	$(SRCDIR)/vqec_dp_syslog.c              \
	$(SRCDIR)/vqec_dp_tlm.c	                \
	$(SRCDIR)/vqec_dp_io_stream.c           \
	$(SRCDIR)/vqec_dp_utils.c               \
        $(SRCDIR)/vqec_dp_debug.c               \
        $(SRCDIR)/vqec_dp_debug_utils.c         \

vqec-dp-common-obj =                            \
	$(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(vqec-dp-common-src:.c=.o)) 

vqec-dp-common-members = 		\
	$(vqec-dp-common-lib)($(vqec-dp-common-obj))



all::	libs build


vqec-dp-subdirs =		\
	channel			\
	inputshim		\
	outputshim		\


#
# Build one library from the combination of these multiple libraries.
#
libs::	$(vqec-dp-common-lib) vqec-dp-nested-lib

#
# common library build rule.
#
$(vqec-dp-common-lib): $(vqec-dp-common-members)
$(vqec-dp-common-lib)(%o): $(MODOBJ)/%.o
	$(AR) crv $@ $^
$(vqec-dp-common-lib)():
	$(AR) crv $(vqec-dp-common-lib) $^


####
# Build PIC libraries
####
pic-dp-common-lib = $(patsubst %.a, %_r.o, $(vqec-dp-common-lib))
$(pic-dp-common-lib): $(vqec-dp-common-obj)
	$(LD) -r -o $@ $^

pic_libs:: CFLAGS += -fPIC
pic_libs:: $(pic-dp-common-lib) pic-dp-nested-lib


#
# nested library build rule.
#
vqec-dp-nested-lib:
	@echo Making nested libraries
	@for dir in $(vqec-dp-subdirs); do		\
		echo "===> $$dir"; 		\
		if ! $(MAKE) -wC $$dir libs; then 	\
			$(MAKE_K_CHECK)	\
		fi				\
	done

#
# nested pic build rule.
#
pic-dp-nested-lib:
	@echo Making nested PIC libraries
	@for dir in $(vqec-dp-subdirs); do			\
		echo "===> $$dir"; 			\
		if ! $(MAKE) -wC $$dir pic_libs; then 	\
			$(MAKE_K_CHECK)		\
		fi					\
	done

#
# vqec dp test app executable build target
#
test-vqec-dp-exe = $(MODOBJ)/test_vqec_dp

vqec-dp-nested-libs = \
	$(LIBDIR)/libvqec-dp-inputshim.a		\
	$(LIBDIR)/libvqec-dp-channel.a			\
	$(LIBDIR)/libvqec-dp-outputshim.a		\

# NOTE: libdebug.a is used below (as opposed to libdebug_vqec.a) ONLY for unit
#       tests; libdebuf_vqec.a should be linked with for all regular builds
test-vqec-dp-dep-lib-names = \
	libdebug.a			\
	libidmgr.a			\
	rtp.a				\
	libmp_tlv.a			\
	libvqecutils.a			\
	libzonemgr.a 			\

test-vqec-dp-deps = \
	$(vqec-dp-common-lib)							\
        $(vqec-dp-nested-libs)                                                  \
        $(patsubst %, $(LIBDIR)/%, $(test-vqec-dp-dep-lib-names))               \
	$(ROOT)/add-ons/lib/$(ARCH)/libevent.a                                  \
        -lpthread                                                               \
	-lssl				\
	-lm  				\

build::	$(test-vqec-dp-exe)
test-vqec-dp-src = 	$(SRCDIR)/test_vqec_dp.c 		\
			$(SRCDIR)/test_dp_interposers.c	\
			$(SRCDIR)/vqec_debug_cp.c		\

test-vqec-dp-obj = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(test-vqec-dp-src:.c=.o))
$(MODOBJ)/test_vqec_dp: $(test-vqec-dp-obj) $(test-vqec-dp-deps)
	$(CC) $(LDFLAGS) $(CFLAGS) $^ $(LIBDIR)/libvqec-dp-common.a -o $@ 

#
# cleanup
#
clean::
	$(RM) $(vqec-dp-common-lib)
	$(RM) $(test-vqec-dp-exe)
	@echo cleaning nested libraries
	@for dir in $(vqec-dp-subdirs); do				\
		echo "===> $$dir"; 				\
		if ! $(MAKE) -wC $$dir $(MAKECMDGOALS); then	\
			$(MAKE_K_CHECK)			\
		fi						\
	done

pic_clean:: 
	$(RM) $(pic-dp-common-lib)

clean-utrun::
	$(RM) $(MODOBJ)/$(vqec-dp-utest-exe)
	$(RM) $(MODOBJ)/../*.o
	@echo cleaning nested utrun libraries
	@for dir in $(vqec-dp-subdirs); do		\
		echo "===> $$dir"; 		\
		if ! $(MAKE) -wC $$dir clean-utrun; then 	\
			$(MAKE_K_CHECK)	\
		fi				\
	done


# -----------------------------------------------------------------------------
# Unit test related stuff.
# -----------------------------------------------------------------------------

VQEC_DP_UTEST_SRC = $(SRCDIR)/test_dp_interposers.c
VQEC_DP_UTEST_OBJ = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(VQEC_DP_UTEST_SRC:.c=.o))
utrun_libs:: $(VQEC_DP_UTEST_OBJ)
	$(AR) crv $(vqec-dp-common-lib) $^

vqec-dp-utest-exe = test_vqec_dp_common_utest
utrun_build::	$(vqec-dp-utest-exe)
.PHONY:	$(vqec-dp-utest-exe)

#
# Unit-test sources.
#
vqec-dp-common-utest-src =	\
        $(SRCDIR)/test_vqec_utest_dp_common.c                   \
        $(SRCDIR)/test_vqec_utest_dp_tlm.c                      \
	$(SRCDIR)/test_vqec_utest_dp_graphbuilder.c		\
        $(SRCDIR)/test_dp_interposers.c                         \
        $(SRCDIR)/test_vqec_utest_dp_utils.c                         \
        $(SRCDIR)/../vqec_pthread.c                             \

#
# Unit-test library dependencies.
#
# NOTE: libdebug.a is used below (as opposed to libdebug_vqec.a) ONLY for unit
#       tests; libdebuf_vqec.a should be linked with for all regular builds
vqec-dp-utest-dep-lib-names = 			\
	libidmgr.a				\
	rtp.a					\
	libvqecutils.a				\
	libmp_tlv.a				\
	libzonemgr.a 				\
	libvqec.a				\
	libdebug.a				\

vqec-dp-nested-lib-paths = \
	$(LIBDIR)/libvqec-dp-inputshim.a	\
	$(LIBDIR)/libvqec-dp-channel.a		\
	$(LIBDIR)/libvqec-dp-outputshim.a	\

vqec-dp-utest-dep-libs = \
        $(patsubst %, $(LIBDIR)/%, $(vqec-dp-utest-dep-lib-names))              \
	$(ROOT)/add-ons/lib/$(ARCH)/libevent.a                                  \
        -lpthread                                                               \
	$(vqec-dp-common-lib)							\
	$(vqec-dp-nested-lib-paths)						\
	-lm									\

vqec-dp-common-utest-obj = 		\
	$(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(vqec-dp-common-utest-src:.c=.o)) 

vqec-dp-utest-libs = $(ROOT)//add-ons/lib/$(ARCH)/libcunit.a $(vqec-dp-utest-dep-libs)

test_vqec_dp_common_utest: $(MODOBJ)/test_vqec_dp_common_utest
$(MODOBJ)/test_vqec_dp_common_utest: $(vqec-dp-common-utest-obj) $(vqec-dp-utest-libs)
	$(CC) $(LDFLAGS) $(CFLAGS) $^ -o $@ $(vqec-dp-utest-libs)


utrun_libs::	CFLAGS +=	-D_VQEC_DP_UTEST
utrun_libs:: nested-utrun-libs
utrun_build::	CFLAGS +=	-D_VQEC_DP_UTEST
utrun_build:: nested-utrun-build

#
# nested utrun-lib rule.
#
nested-utrun-libs:
	@echo Making nested utest libraries
	@for dir in $(vqec-dp-subdirs); do			\
		echo "===> $$dir"; 			\
		if ! $(MAKE) -wC $$dir utrun_libs; then 	\
			$(MAKE_K_CHECK)		\
		fi					\
	done

#
# nested utrun-build rule.
#
nested-utrun-build:
	@echo  Building nested utests
	@for dir in $(vqec-dp-subdirs); do			\
		echo "===> $$dir"; 			\
		if ! $(MAKE) -wC $$dir utrun_build; then 	\
			$(MAKE_K_CHECK)		\
		fi					\
	done


#
# nested run unit-tests.
#
nested-run-utests: nested-utrun-libs nested-utrun-build
	@echo  Running nested utests
	@for dir in $(vqec-dp-subdirs); do			\
		echo "===> $$dir"; 			\
		if ! $(MAKE) -wC $$dir utrun; then		\
			$(MAKE_K_CHECK)		\
		fi					\
	done

#
# run unit-tests for base directory.
#
common-run-utests:
	@echo "Running dataplane common unit-tests"
	$(MODOBJ)/$(vqec-dp-utest-exe)
	$(LCOV_DIR)/lcov --capture --directory $(MODOBJ) -base-directory . --output-file $(MODOBJ)/utest.info --test-name utest
	$(LCOV_DIR)/genhtml $(MODOBJ)/utest.info --output-directory $(MODOBJ)/output --title "dp-common unit tests" --show-details --legend


utrun::	nested-utrun-libs utrun_libs nested-utrun-build utrun_build nested-run-utests common-run-utests


# -----------------------------------------------------------------------------


#
# Sources.
#
SRC +=	$(vqec-dp-common-src) $(vqec-dp-common-utest-src)


#
# always include depend.mk except if target does not need it included
# targets that do not need depend.mk are defined in vam.mk. Copy lines
# below as-is.
#
ifeq ($(include-depend-mk), 1)
include $(ROOT)/depend.mk
endif

