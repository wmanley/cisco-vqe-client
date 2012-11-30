#
# Copyright (c) 2006-2009 by Cisco Systems, Inc.
#

#local include paths go here
INCLUDES +=  -I$(ROOT) -I$(ROOT)/include/utils \
	    -D_LIBC_REENTRANT

CFLAGS +=
 
LIBS +=  $(LIBDIR)/rtp.a $(LIBDIR)/libzonemgr.a $(LIBDIR)/libdebug.a -lpthread \
	 $(ROOT)/add-ons/lib/$(ARCH)/libevent.a -lm

# Build a local library for others to use!
#

LIB_SRC = $(SRCDIR)/rtp_asm.c $(SRCDIR)/rtp_session.c \
	$(SRCDIR)/rtp_ptp.c $(SRCDIR)/rtp_ssm_rsi_source.c \
	$(SRCDIR)/rtp_ssm_rsi_rcvr.c $(SRCDIR)/rtp_util.c \
	$(SRCDIR)/rtp_random.c $(SRCDIR)/rcc_tlv.c $(SRCDIR)/rtp_header.c \
	$(SRCDIR)/rtp_ssm_rsi.c $(SRCDIR)/rtp_ssm_rsi_fbt.c \
	$(SRCDIR)/rtp_syslog.c $(SRCDIR)/rtcp_bandwidth.c \
	$(SRCDIR)/rtcp_statistics.c $(SRCDIR)/rtcp_stats_api.c \
	$(SRCDIR)/rtcp_memory.c $(SRCDIR)/rtp_database.c \
	$(SRCDIR)/rtcp_xr.c

# name of library (gets put into the lib dir)
LIB = $(LIBDIR)/rtp.a

LIB_OBJ = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(LIB_SRC:.c=.o))
LIB_MEMBERS = $(LIB)($(LIB_OBJ))
libs:: $(LIB)
$(LIB): $(LIB_MEMBERS)
$(LIB)(%.o): $(MODOBJ)/%.o
	$(AR) crv $@ $^
$(LIB)():
	$(AR) crv $(LIB) $^

####
# Build PIC library
####
PIC_LIB = $(patsubst %.a, %_r.o, $(LIB))
$(PIC_LIB): $(LIB_OBJ)
	$(LD) -r -o $@ $^

pic_libs:: $(PIC_LIB)


# 
# Build RTP test programs.
# rtp_utest uses CUnit framework
#

utrun:: utrun_libs utrun_build
	@echo "running rtp unit tests"
	$(MODOBJ)/$(TEST_PROGS)
	$(LCOV_DIR)/lcov --capture --directory $(MODOBJ) -base-directory . --output-file $(MODOBJ)/utest.info --test-name utest
	$(LCOV_DIR)/genhtml $(MODOBJ)/utest.info --output-directory $(MODOBJ)/output --title "rtp unit tests" --show-details --legend


TEST_PROGS = rtp_utest
build:: $(TEST_PROGS)
.PHONY: $(TEST_PROGS)

SRC =   $(LIB_SRC) $(SRCDIR)/rtp_testmain.c $(SRCDIR)/rtp_testbase.c \
	$(SRCDIR)/rtp_testasm.c $(SRCDIR)/rtp_testptp.c \
	$(SRCDIR)/rtp_testssmsrc.c $(SRCDIR)/rtp_testssmrcv.c \
	$(SRCDIR)/rtcp_testxr.c

OBJ = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(SRC:.c=.o))  
TESTLIB = $(LIBS) $(ROOT)/add-ons/lib/$(ARCH)/libcunit.a
TESTINC = -I$(ROOT)/add-ons/include/CUnit
CFLAGS += $(TESTINC) -D_UNIT_TEST
LIBS += $(ROOT)/add-ons/lib/$(ARCH)/libcunit.a

rtp_utest: $(MODOBJ)/rtp_utest

$(MODOBJ)/rtp_utest: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS) 


# Clean build. remove libs and objs.  Define PROG_FILES for full path to PROG var.
TEST_PROGS_FILES = $(addprefix $(MODOBJ)/, $(TEST_PROGS))
clean::
	$(RM) $(LIBDIR)/rtp.a
	$(RM) $(TEST_PROGS_FILES)

pic_clean:: 
	$(RM) $(PIC_LIB)

# always include depend.mk except if target does not need it included
# targets that do not need depend.mk are defined in vam.mk. Copy lines
# below as-is.
ifeq ($(include-depend-mk), 1)
include $(ROOT)/depend.mk
endif
