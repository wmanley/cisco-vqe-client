#
# Copyright (c) 2006-2008 by Cisco Systems, Inc.
# All rights reserved.
#

#local include paths go here
INCLUDE_PATHS += $(SRCDIR)      \
        $(MODOBJ)               \
        $(ROOT)                 \
        $(ROOT)/eva             \
        $(ROOT)/utils           \
        $(ROOT)/stun            \
        $(ROOT)/stunclient      \
        $(ROOT)/include         \
        $(ROOT)/include/utils   \
        $(ROOT)/include/log     \
        $(ROOT)/add-ons/include \

INCLUDES = $(addprefix -I, $(INCLUDE_PATHS))

LDFLAGS = -L$(LIBDIR) -L$(ROOT)/add-ons/lib/$(ARCH)

LIB_INCLUDES =   $(INCLUDES)
CFLAGS +=	 $(LIB_INCLUDES)

STUNCLIENT_LIB = $(LIBDIR)/stunclient.a

STUNCLIENT_SRC = \
        $(SRCDIR)/vqec_stun_mgr.c \
        $(SRCDIR)/vqec_upnp_mgr.c \
        $(SRCDIR)/vqec_hybrid_nat_mgr.c \

STUNCLIENT_LIB_OBJ      = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(STUNCLIENT_SRC:.c=.o))
STUNCLIENT_LIB_MEMBERS  = $(STUNCLIENT_LIB)($(STUNCLIENT_LIB_OBJ))

all:: 	libs build

utrun:: utrun_libs utrun_build

utrun_libs:: CFLAGS +=	-D_VQEC_UTEST_INTERPOSERS -I$(ROOT)/eva/vqec-dp -I$(ROOT)/vqecutils -I$(ROOT)/rtp

utrun_build::

.PHONY: $(utrun_libs)

%.d: %.c
	$(CC) -M $(CPPFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	$(RM) $@.$$$$

#
# Libraries built here
#
libs:: $(STUNCLIENT_LIB)

$(STUNCLIENT_LIB): $(STUNCLIENT_LIB_MEMBERS)

$(STUNCLIENT_LIB)(%o): $(MODOBJ)/%.o
	$(AR) crv $@ $^

####
# Build PIC library
####
PIC_LIB = $(patsubst %.a, %_r.o, $(STUNCLIENT_LIB))
$(PIC_LIB): $(STUNCLIENT_LIB_OBJ)
	$(LD) -r -o $@ $^

pic_libs:: $(PIC_LIB)


#
# Applications built here
#
build::

utrun_build::

clean::
	$(RM) $(STUNCLIENT_LIB)

pic_clean:: 
	$(RM) $(PIC_LIB)

#
# Sources
#
SRC += 	$(STUNCLIENT_SRC) 		\

#
# Keep make from deleting intermediate files.
#
sources: $(STUNCLIENT_LIB_OBJ)


# always include depend.mk except if target does not need it included
# targets that do not need depend.mk are defined in vam.mk. Copy lines
# below as-is.
ifeq ($(include-depend-mk), 1)
include $(ROOT)/depend.mk
endif

