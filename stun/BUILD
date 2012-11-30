###############################################################################
# Makefile
#
# Copyright (c) 2006-2008 by Cisco Systems, Inc.
# All rights reserved. 
###############################################################################

#local include paths go here
INCLUDES += -I$(ROOT)/cm -I$(ROOT)/util -I$(ROOT)/cli

SUBDIRS = 

.PHONY: $(SUBDIRS)

all:: libs build


ifneq ($(HAVE_OPENSSL), 1)
 $(warning integrity checks are disabled without openssl)
endif

TEST_PROGS = test_stun_server test_stun_client test_stun_parser
build:: $(TEST_PROGS)
.PHONY: $(TEST_PROGS)

LDFLAGS =

ifeq ($(HAVE_OPENSSL), 1)
	LIBS += -L$(LIBDIR) -lssl
else
	LIBS += -L$(LIBDIR)
endif

%.d: %.c
	$(CC) -M $(CPPFLAGS) $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
		$(RM) $@.$$$$

LIB_SRC = $(SRCDIR)/stun_parse.c $(SRCDIR)/stun_hmac.c \
	  $(SRCDIR)/transaction_id.c $(SRCDIR)/stun_msg.c
# name of library (gets put into the lib dir)
LIB = $(LIBDIR)/stun.a
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


TEST_STUN_SERVER_SRC = $(SRCDIR)/test_stun_server.c $(SRCDIR)/stun_socket.c
TEST_STUN_SERVER_OBJS = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(TEST_STUN_SERVER_SRC:.c=.o))
TEST_STUN_SERVER_OBJS += $(LIBDIR)/stun.a
test_stun_server: $(MODOBJ)/test_stun_server
$(MODOBJ)/test_stun_server: $(TEST_STUN_SERVER_OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^ $(LIBS)

TEST_STUN_CLIENT_SRC = $(SRCDIR)/test_stun_client.c $(SRCDIR)/stun_socket.c
TEST_STUN_CLIENT_OBJS = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(TEST_STUN_CLIENT_SRC:.c=.o))
TEST_STUN_CLIENT_OBJS += $(LIBDIR)/stun.a
test_stun_client: $(MODOBJ)/test_stun_client
$(MODOBJ)/test_stun_client: $(TEST_STUN_CLIENT_OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^ $(LIBS)

TEST_STUN_PARSER_SRC = $(SRCDIR)/test_stun_parser.c
TEST_STUN_PARSER_OBJS = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(TEST_STUN_PARSER_SRC:.c=.o))
TEST_STUN_PARSER_OBJS += $(LIBDIR)/stun.a
test_stun_parser: $(MODOBJ)/test_stun_parser
$(MODOBJ)/test_stun_parser: $(TEST_STUN_PARSER_OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^ $(LIBS)

SRC = $(LIB_SRC) \
      $(TEST_STUN_SERVER_SRC) \
      $(TEST_STUN_CLIENT_SRC) \
      $(TEST_STUN_PARSER_SRC) 

# Clean build. remove libs and objs.  Define PROG_FILES for full path to PROG var.
TEST_PROGS_FILES = $(addprefix $(MODOBJ)/, $(TEST_PROGS))
clean::
	$(RM) $(LIBDIR)/stun.a
	$(RM) $(TEST_PROGS_FILES)

pic_clean:: 
	$(RM) $(PIC_LIB)

# always include depend.mk except if target does not need it included
# targets that do not need depend.mk are defined in vam.mk. Copy lines
# below as-is.
ifeq ($(include-depend-mk), 1)
include $(ROOT)/depend.mk
endif
