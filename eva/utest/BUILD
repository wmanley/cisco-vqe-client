#
# Copyright (c) 2006-2010 by Cisco Systems, Inc.
# All rights reserved
#

include $(ROOT)/eva/vqecmake.mk

CFLAGS += -D_VQEC_UTEST_INTERPOSERS -fprofile-arcs -ftest-coverage

VQEC_DEP_UTEST = -lcunit				

UTRUN_TARGETS =			\
	test_vqec_utest		\

UTRUN_KMOD_TARGETS =      \
	test_vqec_kmod_utest  \

.PHONY: $(UTRUN_TARGETS)
.PHONY: $(UTRUN_KMOD_TARGETS)

VQEC_UTRUN_SRC =                                          \
        $(SRCDIR)/test_vqec_utest_main.c                  \
        $(SRCDIR)/../test_vqec_utest_interposers.c        \
        $(SRCDIR)/test_vqec_utest_bitmap.c                \
        $(SRCDIR)/test_vqec_utest_gaptree.c               \
        $(SRCDIR)/test_vqec_utest_gap_reporter.c          \
        $(SRCDIR)/test_vqec_utest_heap.c                  \
        $(SRCDIR)/test_vqec_utest_recv_socket.c           \
        $(SRCDIR)/test_vqec_utest_url.c                   \
        $(SRCDIR)/test_vqec_utest_pak.c                   \
        $(SRCDIR)/test_vqec_utest_pak_seq.c               \
        $(SRCDIR)/test_vqec_utest_seq_num.c               \
        $(SRCDIR)/test_vqec_utest_hash.c                  \
        $(SRCDIR)/test_vqec_utest_event.c   	          \
        $(SRCDIR)/test_vqec_utest_cli.c                   \
        $(SRCDIR)/test_vqec_utest_tuner.c                 \
        $(SRCDIR)/test_vqec_utest_channel.c               \
        $(SRCDIR)/test_vqec_utest_nat.c                   \
        $(SRCDIR)/test_vqec_utest_igmp.c                  \
        $(SRCDIR)/test_vqec_utest_stream_output.c         \
        $(SRCDIR)/test_vqec_utest_cfg.c                   \
        $(SRCDIR)/test_vqec_utest_debug.c                 \
        $(SRCDIR)/test_vqec_utest_rtp.c                   \
        $(SRCDIR)/test_vqec_utest_port.c                  \
        $(SRCDIR)/test_vqec_utest_sm.c                    \
        $(SRCDIR)/test_vqec_utest_ifclient.c              \
        $(SRCDIR)/test_vqec_utest_updater.c               \
        $(SRCDIR)/test_vqec_utest_upcall.c                \
        $(SRCDIR)/test_vqec_utest_drop.c                  \
        $(SRCDIR)/test_vqec_utest_configparser.c          \
        $(SRCDIR)/test_vqec_utest_failover.c              \

VQEC_UTRUN_KMOD_SRC =                                     \
        $(SRCDIR)/test_kmod_vqec_utest_main.c             \
        $(SRCDIR)/test_kmod_vqec_utest_rpc.c              \
        $(SRCDIR)/../test_vqec_utest_interposers.c        \


#
# TODO:
# Some targets stubbed.  Add them back in.
#
#        $(SRCDIR)/test_vqec_utest_log.c		          \

build:: $(UTRUN_TARGETS)

utrun:: $(UTRUN_TARGETS)
	echo "running vqec unit tests\n"
#  Note zeroing the counters doesn't really work for us as we have .gcda files not .da files, see $(LCOV_DIR)/lcov
#  Do a make clean prior to make utrun to get clean unit testing results.
#       $(LCOV_DIR)/lcov --zerocounters --directory $(MODOBJ) -base-directory .
	cd $(ARCH)-utrun; mkdir -p data; cp -f ../$(SRCDIR)/data/* data; ./test_vqec_utest
	$(RM) $(MODOBJ)/*.gc*
	$(RM) $(MODOBJ)/../../sample/$(ARCH)-utrun/*.gc*
	$(LCOV_DIR)/lcov --capture --directory $(MODOBJ)/../../$(ARCH)-utrun -base-directory ..  --output-file $(MODOBJ)/utest.info --test-name utest --no-checksum
	$(LCOV_DIR)/genhtml $(MODOBJ)/utest.info --output-directory $(MODOBJ)/output --title "vqec unit tests" --show-details --legend

VQEC_UTRUN_OBJS = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(VQEC_UTRUN_SRC:.c=.o))
test_vqec_utest: $(MODOBJ)/test_vqec_utest
$(MODOBJ)/test_vqec_utest: $(VQEC_UTRUN_OBJS) $(VQEC_LIB) $(VQEC_DEP_VQE)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^ $(VQEC_LIB) $(VQEC_DEP_VQE) \
	$(VQEC_DEP_EXTERNAL) $(VQEC_DEP_UTEST) $(VQEC_DEP_ELOG)

utrun-kmod:: $(UTRUN_KMOD_TARGETS)
	echo "compiling kmod vqec unit tests\n"
	cd $(ARCH)-utrun; mkdir -p data; cp -f ../$(SRCDIR)/data/* data
	$(RM) $(MODOBJ)/*.gc*
	$(RM) $(MODOBJ)/../../sample/$(ARCH)-utrun/*.gc*

VQEC_UTRUN_KMOD_OBJS = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(VQEC_UTRUN_KMOD_SRC:.c=.o))
test_vqec_kmod_utest: $(MODOBJ)/test_vqec_kmod_utest
$(MODOBJ)/test_vqec_kmod_utest: $(VQEC_UTRUN_KMOD_OBJS) $(VQEC_LIB_KMOD) $(VQEC_DEP_CP_VQE)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^ $(VQEC_LIB_KMOD) $(VQEC_DEP_CP_VQE) \
	$(VQEC_DEP_EXTERNAL) $(VQEC_DEP_UTEST) $(VQEC_DEP_ELOG)
    

#
# Sources
#
SRC =   $(VQEC_UTRUN_SRC) 

# Clean build. remove libs and objs.  Define UTRUN_TARGETS_FILES for full 
# path to UTRUN_TARGETS var.
UTRUN_TARGETS_FILES = $(addprefix $(MODOBJ)/, $(UTRUN_TARGETS))
UTRUN_KMOD_TARGETS_FILES = $(addprefix $(MODOBJ)/, $(UTRUN_TARGETS))
clean::
	$(RM) $(UTRUN_TARGETS_FILES)
	$(RM) $(UTRUN_KMOD_TARGETS_FILES)
	$(RM) $(MODOBJ)/*.tmp
	$(RMDIR) $(MODOBJ)/data
	$(RM) $(MODOBJ)/../*.o
	$(RM) $(MODOBJ)/../*.gcda
	$(RM) $(MODOBJ)/../*.gcno
	$(RM) $(MODOBJ)/../*.dep

# always include depend.mk except if target does not need it included
# targets that do not need depend.mk are defined in vam.mk. Copy lines
# below as-is.
ifeq ($(include-depend-mk), 1)
include $(ROOT)/depend.mk
endif
