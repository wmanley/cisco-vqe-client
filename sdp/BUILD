#
# Copyright (c) 2006-2008 by Cisco Systems, Inc.
#

#local include paths go here
INCLUDES += 

SUBDIRS = 

.PHONY: $(SUBDIRS)

all:: libs build


# libraries you want to link against - they could be anywhere in the search path,
# or in the $(LIBDIR) dir. $(ROOT) is the vam comp dir.
LIBS += -L$(LIBDIR) -lsdp -lpthread 

#
# Build a local library for others to use!
#

# library source - should be in this dir
LIB_SRC = $(SRCDIR)/sdp_access.c $(SRCDIR)/sdp_attr.c $(SRCDIR)/sdp_attr_access.c \
	  $(SRCDIR)/sdp_config.c $(SRCDIR)/sdp_main.c $(SRCDIR)/sdp_token.c\
	  $(SRCDIR)/sdp_utils.c $(SRCDIR)/sdp_services_unix.c $(SRCDIR)/sdp_base64.c

# name of library (gets put into the lib dir)
LIB = $(LIBDIR)/libsdp.a

# copy the section below as is
LIB_OBJ = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(LIB_SRC:.c=.o))
LIB_MEMBERS = $(LIB)($(LIB_OBJ))
libs:: $(LIB)
$(LIB): $(LIB_MEMBERS)
$(LIB)(%o): $(MODOBJ)/%.o
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


TEST_PROGS = create-channel sdp-test-all

build:: $(TEST_PROGS)
.PHONY: $(TEST_PROGS)

TEST_CREATE_CHANNEL_SRC = $(SRCDIR)/channel_config.c
TEST_CREATE_CHANNEL_OBJS = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(TEST_CREATE_CHANNEL_SRC:.c=.o))
create-channel: $(MODOBJ)/create-channel
$(MODOBJ)/create-channel: $(TEST_CREATE_CHANNEL_OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LIBS)

TEST_SDP_TEST_ALL_SRC = $(SRCDIR)/sdp_test_all.c
TEST_SDP_TEST_ALL_OBJS = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(TEST_SDP_TEST_ALL_SRC:.c=.o))
sdp-test-all: $(MODOBJ)/sdp-test-all
$(MODOBJ)/sdp-test-all: $(TEST_SDP_TEST_ALL_OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LIBS)

%.d: %.c
	$(CC) -M $(CPPFLAGS) $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
		$(RM) $@.$$$$

SRC = $(LIB_SRC) \
      $(TEST_CREATE_CHANNEL_SRC) \
      $(TEST_SDP_TEST_ALL_SRC)


# clean build. Remove libs and exes. Define TEST_PROGS_FILES for full path to PROG var.
TEST_PROGS_FILES = $(addprefix $(MODOBJ)/, $(TEST_PROGS))
clean::
	$(RM) $(LIBDIR)/libsdp.a
	$(RM) $(TEST_PROGS_FILES)

pic_clean:: 
	$(RM) $(PIC_LIB)

# always include depend.mk except if target does not need it included
# targets that do not need depend.mk are defined in vam.mk. Copy lines
# below as-is.
ifeq ($(include-depend-mk), 1)
include $(ROOT)/depend.mk
endif

