#
# Copyright (c) 2006-2008 by Cisco Systems, Inc.
#

#local include paths go here
INCLUDES += -I$(ROOT)/cfg -I$(ROOT)/sdp -I$(ROOT)/eva -I$(ROOT)/include/utils \
-I$(ROOT)/include/log -I$(ROOT)/vqecutils -D_LIBC_REENTRANT 

SUBDIRS = 

.PHONY: $(SUBDIRS)

all:: libs build


# libraries you want to link against - they could be anywhere in the search path,
# or in the $(LIBDIR) dir. $(ROOT) is the vam comp dir.
LIBS += -L$(LIBDIR) -lrtspclient -lcfg -lsdp -lzonemgr -lpthread -lresolv -lidmgr -ldebug

#
# Build a local library for others to use!
#

# library source - should be in this dir
LIB_SRC = $(SRCDIR)/rtsp_client.c \
	$(SRCDIR)/srv_lookup.c \
	$(SRCDIR)/res_copy.c \
	$(SRCDIR)/res_free.c \
	$(SRCDIR)/res_info.c \
	$(SRCDIR)/res_parse.c \
	$(SRCDIR)/res_print.c

# name of library (gets put into the lib dir)
LIB = $(LIBDIR)/librtspclient.a

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


TEST_PROGS = test-rtsp test-res-parse test-multi-rtsp rtspclient-utest

build:: $(TEST_PROGS)
.PHONY: $(TEST_PROGS)

TEST_RTSP_SRC = $(SRCDIR)/test_rtsp.c $(SRCDIR)/rtsp_debug.c
TEST_RTSP_OBJS = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(TEST_RTSP_SRC:.c=.o))
test-rtsp: $(MODOBJ)/test-rtsp
$(MODOBJ)/test-rtsp: $(TEST_RTSP_OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LIBS)


TEST_RES_PARSE_SRC = $(SRCDIR)/testres.c
TEST_RES_PARSE_OBJS = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(TEST_RES_PARSE_SRC:.c=.o))
test-res-parse: $(MODOBJ)/test-res-parse
$(MODOBJ)/test-res-parse: $(TEST_RES_PARSE_OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LIBS)

TEST_MULTI_RTSP_SRC = $(SRCDIR)/multi_rtsp_requests.c  $(SRCDIR)/rtsp_debug.c
TEST_MULTI_RTSP_OBJS = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(TEST_MULTI_RTSP_SRC:.c=.o))
test-multi-rtsp: $(MODOBJ)/test-multi-rtsp
$(MODOBJ)/test-multi-rtsp: $(TEST_MULTI_RTSP_OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LIBS)

UTEST_SRC = $(SRCDIR)/cunit4rtspclient.c $(SRCDIR)/rtspclient_utests.c  $(SRCDIR)/rtsp_debug.c
UTEST_OBJ = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(UTEST_SRC:.c=.o))  
UTEST_INC = -I$(ROOT)/add-ons/include/CUnit
UTEST_LIB = $(ROOT)/add-ons/lib/$(ARCH)/libcunit.a
rtspclient-utest: $(MODOBJ)/rtspclient-utest
$(MODOBJ)/rtspclient-utest: $(UTEST_OBJ)
	$(CC) $(LDFLAGS) $(CFLAGS) $(INCLUDES) $(UTEST_INC) $^ -o $@ \
	$(UTEST_LIB) $(LIBS)

%.d: %.c
	$(CC) -M $(CPPFLAGS) $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
		$(RM) $@.$$$$


# Define SRC for dependency generation
SRC = $(LIB_SRC) \
      $(TEST_RTSP_SRC) \
      $(TEST_RES_PARSE_SRC) \
      $(TEST_MULTI_RTSP_SRC)


# Clean build. remove libs and objs.  Define PROG_FILES for full path to PROG var.
TEST_PROGS_FILES = $(addprefix $(MODOBJ)/, $(TEST_PROGS))
clean::
	$(RM) $(LIB)
	$(RM) $(TEST_PROGS_FILES)

pic_clean:: 
	$(RM) $(PIC_LIB)

# always include depend.mk except if target does not need it included
# targets that do not need depend.mk are defined in vam.mk. Copy lines
# below as-is.
ifeq ($(include-depend-mk), 1)
include $(ROOT)/depend.mk
endif

