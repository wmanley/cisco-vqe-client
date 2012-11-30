#
# Copyright (c) 2006-2009 by Cisco Systems, Inc.
# All rights reserved.
#

include $(ROOT)/vqecutils/utilsmake.mk

LIB_INCLUDES = $(INCLUDES)
CFLAGS +=	 $(LIB_INCLUDES)

VQECUTILS_SRC =                                   \
        $(SRCDIR)/vqec_recv_socket.c              \
        $(SRCDIR)/vqec_event.c                    \
        $(SRCDIR)/vqec_pak.c                      \
        $(SRCDIR)/vqec_pak_seq.c                  \
        $(SRCDIR)/test_vqec_interposers_common.c  \
        $(SRCDIR)/vqec_debug.c                    \

VQECUTILS_LIB_OBJ 	 	= $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(VQECUTILS_SRC:.c=.o))
VQECUTILS_LIB_MEMBERS 	= $(VQECUTILS_LIB)($(VQECUTILS_LIB_OBJ))

all:: 	libs build

#
# The top-level Makefile calls either
#  libs build
# or
#  utrun_libs utrun_build
# vqec.mk defines utrun_build to point to build, but
# we only want to do a regular 'build' when we're NOT building
# unit tests, so we'll keep track of what target
# was actually called here, and act accordingly.
#
BUILDING_UTESTS = 0

utrun_libs:: CFLAGS +=	-D_VQEC_UTEST_INTERPOSERS -I$(ROOT)/eva/vqec-dp

utrun_build:: BUILDING_UTESTS = 1

.PHONY: $(utrun_libs)

#
# Libraries built here
#
libs:: $(VQECUTILS_LIB)

$(VQECUTILS_LIB): $(VQECUTILS_LIB_MEMBERS)
$(VQECUTILS_LIB)(%o): $(MODOBJ)/%.o
	$(AR) crv $@ $^
$(VQECUTILS_LIB)():
	$(AR) crv $(VQECUTILS_LIB) $^

####
# Build PIC library
####
PIC_LIB = $(patsubst %.a, %_r.o, $(VQECUTILS_LIB))
$(PIC_LIB): $(VQECUTILS_LIB_OBJ)
	$(LD) -r -o $@ $^

pic_libs:: $(PIC_LIB)

clean::
	$(RM) $(VQECUTILS_LIB)

pic_clean:: 
	$(RM) $(PIC_LIB) 

#
# Sources
#
SRC += 	$(VQECUTILS_SRC)

# always include depend.mk except if target does not need it included
# targets that do not need depend.mk are defined in vam.mk. Copy lines
# below as-is.
ifeq ($(include-depend-mk), 1)
include $(ROOT)/depend.mk
endif

