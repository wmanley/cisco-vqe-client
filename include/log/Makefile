#
# Copyright (c) 2006-2008 by Cisco Systems, Inc.
# All Rights Reserved.
#
# Makefile to generate vqes syslog_def.html file from
# syslog header file.
#
include ../../vqe.mk

# Define CFLAGS based on build target. Mainly to distinguish between Release 
# and Development builds so definitions not needed for FCS builds are not
# generated.

# Default CFLAGS
CFLAGS = -D_SYSLOGD_ONLY -DSYSLOG_DOCGEN

# source-mode target
ifneq "$(findstring source-mode, $(MAKECMDGOALS))" ""
HAVE_SOURCE_MODE = 1
CFLAGS += -DHAVE_SOURCE_MODE
endif

# Export CFLAGS to be used by doc generation script.
export CFLAGS := $(CFLAGS)

all:: vqes-syslog-docgen
source-mode:: all

SYSLOG_DOCGEN = $(ROOT)/include/log/syslog_docgen
RM            = /bin/rm -f

vqes-syslog-docgen:: clean
	@echo ""
	@echo "Generating syslog_def.html...."
	@echo ""
	@$(SYSLOG_DOCGEN)

clean::
	$(RM) syslog_all.h
	$(RM) syslog_doc.h
	$(RM) syslog.def
	$(RM) syslog_def.html
