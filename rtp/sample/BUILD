#
# Copyright (c) 2006-2008 by Cisco Systems, Inc.
# All rights reserved
#

#local include paths go here
INCLUDES +=  -I$(ROOT)/include/utils -I$(ROOT)/rtp -I$(ROOT)/exporter \
             -I$(ROOT)/eva -I$(ROOT)/eva/vqec-dp -I$(ROOT)/vqecutils \
             -I$(ROOT)/include/log
CFLAGS += 
 
LIBS +=  $(LIBDIR)/rtp.a \
	 $(LIBDIR)/exporter.a \
	 $(LIBDIR)/libzonemgr.a \
         $(LIBDIR)/libdebug.a \
         -lpthread -lssl $(ROOT)/add-ons/lib/$(ARCH)/libevent.a

# Build a local library for others to use!
#

LIB_SRC = $(SRCDIR)/sample_rtp_dataplane.c \
          $(SRCDIR)/sample_rtp_session.c   \
          $(SRCDIR)/sample_rtp_channel.c   \
	  $(SRCDIR)/sample_rtp_utils.c     \
          $(SRCDIR)/sample_rtp_script.c    \
          $(SRCDIR)/sample_rtcp_script.c

# 
# Build RTP test programs.
# rtp-test  tests rtp api
#

TEST_PROGS = sample_session sample_sink rtcp_interval importer
build:: $(TEST_PROGS)
.PHONY: $(TEST_PROGS)


# sample_session
SAMPLE_SESSION_SRC = $(LIB_SRC) $(SRCDIR)/sample_rtp_application.c 
SAMPLE_SESSION_OBJ = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(SAMPLE_SESSION_SRC:.c=.o))  

# sample_sink
SAMPLE_SINK_SRC = $(LIB_SRC) $(SRCDIR)/sample_sink.c 
SAMPLE_SINK_OBJ = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(SAMPLE_SINK_SRC:.c=.o))

# importer
SAMPLE_IMPORTER_SRC = $(LIB_SRC) $(SRCDIR)/sample_importer.c 
SAMPLE_IMPORTER_OBJ = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(SAMPLE_IMPORTER_SRC:.c=.o))

# rtcp_interval
RTCP_INTERVAL_SRC = $(LIB_SRC) $(SRCDIR)/rtcp_interval.c 
RTCP_INTERVAL_OBJ = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(RTCP_INTERVAL_SRC:.c=.o))  

sample_session: $(MODOBJ)/sample_session
$(MODOBJ)/sample_session: $(SAMPLE_SESSION_OBJ)
	$(CC) $(LDFLAGS) $(CFLAGS) $^ -o $@ $(LIBS)

sample_sink: $(MODOBJ)/sample_sink
$(MODOBJ)/sample_sink: $(SAMPLE_SINK_OBJ)
	$(CC) $(LDFLAGS) $(CFLAGS) $^ -o $@ $(LIBS)

importer: $(MODOBJ)/importer
$(MODOBJ)/importer: $(SAMPLE_IMPORTER_OBJ)
	$(CC) $(LDFLAGS) $(CFLAGS) $^ -o $@ $(LIBS)

rtcp_interval: $(MODOBJ)/rtcp_interval
$(MODOBJ)/rtcp_interval: $(RTCP_INTERVAL_OBJ)
	$(CC) $(LDFLAGS) $(CFLAGS) $^ -o $@ $(LIBS)

SRC = $(SAMPLE_SESSION_SRC) $(SAMPLE_SINK_SRC)  $(SAMPLE_IMPORTER_SRC) $(LIB_SRC)


# clean build. Remove libs and exes Define PROG_FILES for full path to PROG var.
TEST_PROGS_FILES = $(addprefix $(MODOBJ)/, $(TEST_PROGS))

# Note that that *.o file cleanup is already specified in vam.mk
clean::
	rm -f $(TEST_PROGS_FILES)


# always include depend.mk except if target does not need it included
# targets that do not need depend.mk are defined in vam.mk. Copy lines
# below as-is.
ifeq ($(include-depend-mk), 1)
include $(ROOT)/depend.mk
endif
