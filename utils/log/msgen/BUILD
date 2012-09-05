#
# Copyright (c) 2006-2008 by Cisco Systems, Inc.
#

#local include paths go here
INCLUDES += -I$(ROOT) -I$(ROOT)/xmlrpc \
        -I$(ROOT)/add-ons/include \
	-D_LIBC_REENTRANT

# Override what's defined in vqe.mk
LOCAL_CFLAGS =
CFLAGS += 

SUBDIRS = 




# libraries you want to link against - they could be anywhere in the 
# search path, or in the $(LIBDIR) dir. $(ROOT) is the vam comp dir.
LIBS += $(LIBDIR)/libxmlrpcs.a \
	$(ROOT)/add-ons/lib/$(ARCH)/libevent.a \
	$(ROOT)/add-ons/lib/$(ARCH)/libxmlrpc_server.a \
	$(ROOT)/add-ons/lib/$(ARCH)/libxmlrpc_server_abyss.a \
	$(ROOT)/add-ons/lib/$(ARCH)/libxmlrpc.a \
	$(ROOT)/add-ons/lib/$(ARCH)/libxmlrpc_abyss.a \
	$(ROOT)/add-ons/lib/$(ARCH)/libxmlrpc_util.a \
	$(ROOT)/add-ons/lib/$(ARCH)/libxmlrpc_xmlparse.a \
	$(ROOT)/add-ons/lib/$(ARCH)/libxmlrpc_xmltok.a \
	$(LIBDIR)/libdebug.a \
	$(LIBDIR)/libzonemgr.a \
	-lpthread -lm -lssl

PROGS = msgen

build:: $(PROGS)

.PHONY: $(PROGS)


PROG_SRC =      $(SRCDIR)/msgen_main.c \
                $(SRCDIR)/msgen_xmlrpc.c \
                $(SRCDIR)/msgen_para.c
PROG_OBJS = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(PROG_SRC:.c=.o))

msgen: $(MODOBJ)/msgen
$(MODOBJ)/msgen: $(PROG_OBJS) $(LIBS)
	$(CC) $(LDFLAGS) $(CFLAGS) $^ -o $@

%.d: %.c
	$(CC) -M $(CPPFLAGS) $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
		$(RM) $@.$$$$

.PHONY: $(SUBDIRS)

all:: libs build


# clean build. Remove libs and exes Define PROG_FILES for full path to 
# PROG var.
PROGS_FILES = $(addprefix $(MODOBJ)/, $(PROGS))
clean::
	$(RM) *.o $(PROGS_FILES) $(LIB)


SRC = $(PROG_SRC)

# always include depend.mk except if target does not need it included
# targets that do not need depend.mk are defined in vam.mk. Copy lines
# below as-is.
ifeq ($(include-depend-mk), 1)
include $(ROOT)/depend.mk
endif

