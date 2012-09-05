#
# Copyright (c) 2006-2008 by Cisco Systems, Inc.
#

#local include paths go here
INCLUDES += -I$(ROOT)/sdp

ifeq ($(HAVE_SYSLOG),1)
  INCLUDES += -D_LIBC_REENTRANT
endif

SUBDIRS = 

utrun:: utrun_libs utrun_build
	cp $(SRCDIR)/vam.cfg .
	cp $(SRCDIR)/update.cfg .
	echo "running cfg unit tests\n"
#  Note zeroing the counters doesn't really work for us as we have .gcda files not .da files, see $(LCOV_DIR)/lcov
#  Do a make clean prior to make utrun to get clean unit testing results.
#       $(LCOV_DIR)/lcov --zerocounters --directory $(MODOBJ) -base-directory .
	$(MODOBJ)/cfg-utest
	$(LCOV_DIR)/lcov --capture --directory $(MODOBJ) -base-directory . --output-file $(MODOBJ)/utest.info --test-name utest
	$(LCOV_DIR)/genhtml $(MODOBJ)/utest.info --output-directory $(MODOBJ)/output --title "cfg unit tests" --show-details --legend

.PHONY: $(SUBDIRS) 

all:: libs build

# libraries you want to link against - they could be anywhere in the search path,
# or in the $(ROOT)/lib dir. $(ROOT) is the vam comp dir.
LIBS += -L$(LIBDIR) -lcfg -lsdp -lzonemgr -lpthread -lidmgr -ldebug

#
# Build a local library for others to use!
#

# library source - should be in this dir
LIB_SRC = $(SRCDIR)/cfg_config.c $(SRCDIR)/cfg_channel.c $(SRCDIR)/cfg_database.c

# name of library (gets put into the lib dir)
LIB = $(LIBDIR)/libcfg.a

# copy the section below as is
LIB_OBJ = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(LIB_SRC:.c=.o))
LIB_MEMBERS = $(LIB)($(LIB_OBJ))
libs:: $(LIB)
$(LIB): $(LIB_MEMBERS)
$(LIB)(%o): $(MODOBJ)/%.o
	$(AR) crv $@ $^

####
# Build PIC library
####
PIC_LIB = $(patsubst %.a, %_r.o, $(LIB))
$(PIC_LIB): $(LIB_OBJ)
	$(LD) -r -o $@ $^

pic_libs:: $(PIC_LIB)


TEST_PROGS = test-config gen-config cfg-utest test-cfg-update sdp2xml

build:: $(TEST_PROGS)
.PHONY: $(TEST_PROGS)

TEST_CONFIGURATION_SRC = $(SRCDIR)/configuration_test.c
TEST_CONFIGURATION_OBJS = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(TEST_CONFIGURATION_SRC:.c=.o))
test-config: $(MODOBJ)/test-config
$(MODOBJ)/test-config: $(TEST_CONFIGURATION_OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LIBS)

TEST_CFG_UPDATE_SRC = $(SRCDIR)/configuration_update.c
TEST_CFG_UPDATE_OBJS = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(TEST_CFG_UPDATE_SRC:.c=.o))
test-cfg-update: $(MODOBJ)/test-cfg-update
$(MODOBJ)/test-cfg-update: $(TEST_CFG_UPDATE_OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LIBS)

GENERATE_CONFIGURATION_SRC = $(SRCDIR)/cfg_gen.c
GENERATE_CONFIGURATION_OBJS = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(GENERATE_CONFIGURATION_SRC:.c=.o))
gen-config: $(MODOBJ)/gen-config
$(MODOBJ)/gen-config: $(GENERATE_CONFIGURATION_OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LIBS)

UTEST_SRC = $(SRCDIR)/cunit4cfg.c $(SRCDIR)/cfg_utests.c
UTEST_OBJ = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(UTEST_SRC:.c=.o))  
UTEST_INC = -I$(ROOT)/add-ons/include/CUnit
UTEST_LIB = $(ROOT)/add-ons/lib/$(ARCH)/libcunit.a
cfg-utest: $(MODOBJ)/cfg-utest
$(MODOBJ)/cfg-utest: $(UTEST_OBJ)
	$(CC) $(LDFLAGS) $(CFLAGS) $(INCLUDES) $(UTEST_INC) $^ -o $@ \
	$(UTEST_LIB) $(LIBS)

CONV_SDP_TO_XML_SRC = $(SRCDIR)/cfg_xml.c $(SRCDIR)/sdpconv.c
CONV_SDP_TO_XML_OBJ = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(CONV_SDP_TO_XML_SRC:.c=.o))  
sdp2xml: $(MODOBJ)/sdp2xml
$(MODOBJ)/sdp2xml: $(CONV_SDP_TO_XML_OBJ)
	$(CC) $(LDFLAGS) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LIBS)


%.d: %.c
	$(CC) -M $(CPPFLAGS) $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
		$(RM) $@.$$$$


SRC = $(LIB_SRC) \
      $(TEST_CONFIGURATION_SRC) \
      $(TEST_CFG_UPDATE_SRC) \
      $(GENERATE_CONFIGURATION_SRC) \
      $(CONV_SDP_TO_XML_SRC) \
      $(UTEST_SRC)

# clean build. Remove libs and exes Define PROG_FILES for full path to PROG var.
TEST_PROGS_FILES = $(addprefix $(MODOBJ)/, $(TEST_PROGS))
clean::
	$(RM) $(LIBDIR)/libcfg.a
	$(RM) $(TEST_PROGS_FILES)

pic_clean:: 
	$(RM) $(PIC_LIB)

clean-utrun::
	$(RM) $(PROD_OBJ_ROOT)/$(MODULE)/*.cfg


# always include depend.mk except if target does not need it included
# targets that do not need depend.mk are defined in vam.mk. Copy lines
# below as-is.
ifeq ($(include-depend-mk), 1)
include $(ROOT)/depend.mk
endif

