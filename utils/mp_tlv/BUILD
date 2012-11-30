###############################################################################
# BUILD FILE
#
# Copyright (c) 2006-2008 by Cisco Systems, Inc.
# All rights reserved. 
###############################################################################

#local include paths go here
INCLUDES += -D_LIBC_REENTRANT -I$(ROOT)/include/utils

# Override what's defined in vam.mk
CFLAGS	+= $(INCLUDES)


# libraries you want to link against 
# or in the $(LIBDIR) dir. $(ROOT) is the vam comp dir.
LIBS += -L$(LIBDIR) # -ldebug

#
# Build a local library for others to use!
#

# library source - should be in this dir
LIB_SRC = $(SRCDIR)/mp_tlv.c $(SRCDIR)/mp_tlv_decode.c $(SRCDIR)/mp_tlv_encode.c

# TODO: Add syslog support later
#ifeq ($(HAVE_SYSLOG), 1)
# LIB_SRC += vqes_syslog.c
#else
# LIB_SRC += vqec_syslog_stubs.c
#endif

# name of library (gets put into the lib dir)
LIB = $(LIBDIR)/libmp_tlv.a

# copy the section below as is
LIB_OBJ = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(LIB_SRC:.c=.o))
LIB_MEMBERS = $(LIB)($(LIB_OBJ))

libs:: $(LIB) $(LIB_WITH_STUBS)
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

# 
# Build some executable (primary) - will go into obj/($DIR)
#

# name of the execs to build, copy build, PHONY rules, prog-name rules
#PROG = test_mp_tlv

#build:: $(PROG)
#.PHONY: $(PROG)


# 
# Build another (secondary) executable - will go into obj/($DIR)
#

# clean build. Remove libs and exes Define PROG_FILES for full path to PROG var.
PROG_FILES = $(addprefix $(MODOBJ)/, $(PROG))
# clean build. Remove libs and exes
clean::
	$(RM) $(LIBDIR)/libmp_tlv.a
	$(RM) $(PROG_FILES)

pic_clean:: 
	$(RM) $(PIC_LIB)

SRC = $(LIB_SRC) \

# always include depend.mk except if target does not need it included
# targets that do not need depend.mk are defined in vam.mk. Copy lines
# below as-is.
ifeq ($(include-depend-mk), 1)
include $(ROOT)/depend.mk
endif
