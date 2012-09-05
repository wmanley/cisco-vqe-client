###############################################################################
# Configuration file for all VQE makefiles. All build macros and variables here
#
# Copyright (c) 2006-2008 by Cisco Systems, Inc.
# All rights reserved. 
###############################################################################

# Global Build variables. These will set what product is being built.
PROD_OBJ_ROOT=           $(shell until [ -f vqe?.mk ]; do cd ..; done; /bin/pwd)
BUILD_PRODUCT=           $(basename $(shell until [ -f vqe?.mk ]; do cd ..; done; ls vqe?.mk))
MODULE:=         $(subst $(PROD_OBJ_ROOT),,$(shell /bin/pwd))
SRCDIR= $(ROOT)$(MODULE)
LCOV_DIR=       $(ROOT)/tools

# Global build tool variables. Define them here if they were not defined
# earlier. In some cases like they may be set via environ.mk and we need
# to ensure that they are not reset here.
ifndef CC
CC = gcc
endif

ifndef CPP
CPP = gcc -E
endif

ifndef CXX
CXX = g++
endif

ifndef AR
AR = ar
endif

ifndef LD
LD  = ld
endif

ifndef STRIP
STRIP  = strip
endif

RM              = /bin/rm -f
RMDIR           = /bin/rm -rf

MKID = /router/bin/mkid32

LOCAL_CFLAGS	= -O
CFLAGS+=        -g -Wall -Werror $(LOCAL_CFLAGS) $(INCLUDES)
CXXFLAGS+=      $(CFLAGS)

INCLUDES=	-I$(ROOT)/include

# Define build targets here which will be dependent on product variables
# defined above.
# TARGETOBJDIR will be different for each build target so we can build multiple
# targets in the same workspace without having to do a make clean between
# different target builds. This will be dev for default (development) target
# and for non default targets it will be same as target name.
# MODOBJ will be redefined based on TARGETOBJDIR
TARGETOBJDIR = .

# pic target
ifneq "$(findstring pic, $(MAKECMDGOALS))" ""
TARGETOBJDIR= pic
endif

CFLAGS += 

MODOBJ = $(PROD_OBJ_ROOT)$(MODULE)/$(TARGETOBJDIR)
LIBDIR = $(ROOT)/$(BUILD_PRODUCT)_lib/$(TARGETOBJDIR)

INCLUDES += -I$(SRCDIR) -I$(MODOBJ)

.PHONY:         all libs build clean

all:: $(MODOBJ) libs build

libs:: $(MODOBJ)
	@test -d $(LIBDIR) || mkdir -p $(LIBDIR)

build:: $(MODOBJ)

clean::
	$(RM) $(MODOBJ)/*.o
	$(RM) $(MODOBJ)/*.dep*

# Define pic build targets.
pic:: 		$(MODOBJ) pic_libs pic_build
clean-pic:: 	pic_clean
pic_libs:: 	CFLAGS += -fPIC
pic_libs::		$(MODOBJ)
		@test -d $(LIBDIR) || mkdir -p $(LIBDIR)
pic_build::
pic_clean::	clean

.PHONY: $(MODOBJ)

$(MODOBJ):
	@test -d $(MODOBJ) || mkdir $(MODOBJ)

$(MODOBJ)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(MODOBJ)/%.o: $(SRCDIR)/%.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

$(MODOBJ)/%.opp: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<


%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $(MODOBJ)/$@ $<

%.o: $(SRCDIR)/%.cpp
	$(CC) $(CFLAGS) -c -o $(MODOBJ)/$@ $<

%.opp: $(SRCDIR)/%.cpp
	$(CC) $(CXXFLAGS) -c -o $(MODOBJ)/$@ $<



# Define all targets which will not load the depend.mk file.
no-depend-mk-include-target := clean

include-depend-mk := 1

ifneq "$(findstring $(no-depend-mk-include-target), $(MAKECMDGOALS))" ""
	include-depend-mk := 0
endif
