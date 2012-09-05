###############################################################################
# BUILD FILE
#
# Copyright (c) 2006-2009 by Cisco Systems, Inc.
# All rights reserved. 
###############################################################################

#local include paths go here
INCLUDES += -I$(ROOT) -I$(ROOT)/utils/log \
        -D_LIBC_REENTRANT

# Override what's defined in vam.mk
CFLAGS  += $(INCLUDES)

# libraries you want to link against 
# or in the $(LIBDIR) dir. $(ROOT) is the vam comp dir.
LIBS += -L$(LIBDIR) -ldebug

#
# Build a local library for others to use!
#

# library source - should be in this dir
LIB_SRC = $(SRCDIR)/libdebug.c  \
	$(SRCDIR)/vqes_syslog.c  \
	$(SRCDIR)/vqes_syslog_limit.c

# stubs library src - used for VQEC
LIB_WITH_STUBS_SRC =  		\
	$(SRCDIR)/libdebug.c  		\
	$(SRCDIR)/vqec_syslog_stubs.c	\
	$(SRCDIR)/vqes_syslog_limit.c

# name of library (gets put into the lib dir)
LIB = $(LIBDIR)/libdebug.a
LIB_WITH_STUBS = $(LIBDIR)/libdebug_vqec.a

# copy the section below as is
LIB_OBJ = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(LIB_SRC:.c=.o))
LIB_MEMBERS = $(LIB)($(LIB_OBJ))

LIB_WITH_STUBS_OBJ = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(LIB_WITH_STUBS_SRC:.c=.o))
LIB_WITH_STUBS_MEMBERS = $(LIB_WITH_STUBS)($(LIB_WITH_STUBS_OBJ))

libs:: $(LIB) $(LIB_WITH_STUBS)
$(LIB): $(LIB_MEMBERS)
$(LIB)(%o): $(MODOBJ)/%.o
	$(AR) crv $@ $^

$(LIB_WITH_STUBS): $(LIB_WITH_STUBS_MEMBERS)
$(LIB_WITH_STUBS)(%o): $(MODOBJ)/%.o
	$(AR) crv $@ $^

####
# Build PIC library
####
PIC_LIB = $(LIBDIR)/libdebug_vqec_r.o
$(PIC_LIB): $(LIB_WITH_STUBS_OBJ)
	$(LD) -r -o $@ $^

pic_libs:: $(PIC_LIB)


# 
# Build some executable (primary) - will go into vqes_obj/($DIR)
#

# name of the execs to build, copy build, PHONY rules, prog-name rules
PROG = vqes_logger vqes_debug_test example_debug ex_syslog

build:: $(PROG)
.PHONY: $(PROG)

VQES_LOGGER_SRC = $(SRCDIR)/vqes_logger.c
VQES_LOGGER_OBJS = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(VQES_LOGGER_SRC:.c=.o))
vqes_logger: $(MODOBJ)/vqes_logger
$(MODOBJ)/vqes_logger: $(VQES_LOGGER_OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) $^ -o $@ $(LIBS)

VQES_DEBUG_TEST_SRC = $(SRCDIR)/vqes_debug_test_main.c $(SRCDIR)/vqes_debug_test_cp.c \
			$(SRCDIR)/vqes_debug_test_rtp.c
VQES_DEBUG_TEST_OBJS = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(VQES_DEBUG_TEST_SRC:.c=.o))
vqes_debug_test: $(MODOBJ)/vqes_debug_test
$(MODOBJ)/vqes_debug_test: $(VQES_DEBUG_TEST_OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) $^ -o $@ $(LIBS)

EXAMPLE_DEBUG_SRC = $(SRCDIR)/example_debug.c
EXAMPLE_DEBUG_OBJS = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(EXAMPLE_DEBUG_SRC:.c=.o))
example_debug: $(MODOBJ)/example_debug
$(MODOBJ)/example_debug: $(EXAMPLE_DEBUG_OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) $^ -o $@ $(LIBS)

EX_SYSLOG_SRC = $(SRCDIR)/ex_syslog.c
EX_SYSLOG_OBJS = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(EX_SYSLOG_SRC:.c=.o))
ex_syslog: $(MODOBJ)/ex_syslog
$(MODOBJ)/ex_syslog: $(EX_SYSLOG_OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) $^ -o $@ $(LIBS)


#
# Build utrun test for the rate limited logging library (vqes_syslog_limit.c)
#
utrun_build:: test_vqes_rl_logging
utrun:: utrun_libs utrun_build
	@echo "running rate limited logging unit tests"
	$(MODOBJ)/test_vqes_rl_logging
	$(LCOV_DIR)/lcov --capture --directory $(MODOBJ) -base-directory . --output-file $(MODOBJ)/utest.info --test-name utest
	$(LCOV_DIR)/genhtml $(MODOBJ)/utest.info --output-directory $(MODOBJ)/output --title "rate limited logging unit tests" --show-details --legend

#
# define utrun test objects
#
# These are the object files corresponding to the sources, plus the
# libcunit.a library
#
TEST_VQES_RL_LOGGING_UTEST_OBJS = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(TEST_VQES_RL_LOGGING_UTEST_SRC:.c=.o))
TEST_VQES_RL_LOGGING_UTEST_OBJS += $(ROOT)/add-ons/lib/$(ARCH)/libcunit.a \
                                   $(LIBDIR)/libdebug.a \
                                   $(LIBDIR)/libzonemgr.a

#
# define utrun test sources
#
TEST_VQES_RL_LOGGING_UTEST_SRC = $(SRCDIR)/utest_rl_logging_main.c \
                                $(SRCDIR)/utest_rl_logging_tests.c

#
# define how to build the utrun test program
#
test_vqes_rl_logging: $(MODOBJ)/test_vqes_rl_logging
$(MODOBJ)/test_vqes_rl_logging: $(TEST_VQES_RL_LOGGING_UTEST_OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) $(INCLUDES) -o $@ $^


# 
# Build another (secondary) executable - will go into obj/($DIR)
#

# clean build. Remove libs and exes Define PROG_FILES for full path to PROG var.
PROG_FILES = $(addprefix $(MODOBJ)/, $(PROG))
clean::
	$(RM) $(LIBDIR)/libdebug.a
	$(RM) $(LIBDIR)/libdebug_vqec.a
	$(RM) $(PROG_FILES)

pic_clean:: 
	$(RM) $(PIC_LIB)

SRC = $(LIB_SRC) \
      $(LIB_WITH_STUBS_SRC) \
      $(VQES_LOGGER_SRC) \
      $(VQES_DEBUG_TEST_SRC) \
      $(EXAMPLE_DEBUG_SRC) \
      $(EX_SYSLOG_SRC)


# always include depend.mk except if target does not need it included
# targets that do not need depend.mk are defined in vam.mk. Copy lines
# below as-is.
ifeq ($(include-depend-mk), 1)
include $(ROOT)/depend.mk
endif
