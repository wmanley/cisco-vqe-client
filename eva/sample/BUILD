#
# Copyright (c) 2006-2009 by Cisco Systems, Inc.
# All rights reserved
#

include $(ROOT)/eva/vqecmake.mk

TEST_MODULE_TARGETS = 		             \
	test_vqec_ifclient 		     \
        test_vqec_ifclient_kmod	             \
	test_vqec_ifclient_allso             \

.PHONY: $(TEST_MODULE_TARGETS)

build:: $(TEST_MODULE_TARGETS)

#
# For the following unit-test-extension programs, include dependencies
# as specified in the main makefile, instead of including them individually. 
#
## primary test applications ##
TEST_VQEC_IFCLIENT_SRC = $(SRCDIR)/test_vqec_ifclient.c
TEST_VQEC_IFCLIENT_OBJ = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(TEST_VQEC_IFCLIENT_SRC:.c=.o))
test_vqec_ifclient: $(MODOBJ)/test_vqec_ifclient
$(MODOBJ)/test_vqec_ifclient: $(TEST_VQEC_IFCLIENT_OBJ) $(VQEC_LIB) $(VQEC_DEP_VQE)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^ \
	$(VQEC_DEP_ELOG) $(VQEC_DEP_EXTERNAL)
	cp $(MODOBJ)/test_vqec_ifclient $(MODOBJ)/test_vqec_ifclient_with_debug_symbols
	$(STRIP) $(MODOBJ)/test_vqec_ifclient

test_vqec_ifclient_kmod: $(MODOBJ)/test_vqec_ifclient_kmod
$(MODOBJ)/test_vqec_ifclient_kmod: $(TEST_VQEC_IFCLIENT_OBJ) $(VQEC_LIB_KMOD) $(VQEC_DEP_CP_VQE)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^ \
	$(VQEC_DEP_ELOG) $(VQEC_DEP_EXTERNAL)
	cp $(MODOBJ)/test_vqec_ifclient_kmod $(MODOBJ)/test_vqec_ifclient_kmod_with_debug_symbols
	$(STRIP) $(MODOBJ)/test_vqec_ifclient_kmod

test_vqec_ifclient_allso: $(MODOBJ)/test_vqec_ifclient_allso
$(MODOBJ)/test_vqec_ifclient_allso: $(TEST_VQEC_IFCLIENT_OBJ) $(VQEC_LIB) $(VQEC_DEP_VQE_SO)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^ \
	$(VQEC_DEP_ELOG) $(VQEC_DEP_EXTERNAL_SO)
	cp $(MODOBJ)/test_vqec_ifclient_allso $(MODOBJ)/test_vqec_ifclient_allso_with_debug_symbols
	$(STRIP) $(MODOBJ)/test_vqec_ifclient_allso

#
# Relocatable .so library / test client based on the relocatable library.
#
define BUILD_NUMBER_STR 
 grep -e "VQEC_VAR_BUILD_NUMBER"
endef

define BUILD_NUMBER_VAL 
  sed 's/\#define VQEC_VAR_BUILD_NUMBER\s//g'
endef

define PROD_NUMBER_STR 
 grep -e "VQEC_PRODUCT_VERSION"
endef

define PROD_NUMBER_VAL 
  sed 's/\#define VQEC_PRODUCT_VERSION\s//g'
endef

define VQEC_VMAJ_STR 
 grep -e "VQEC_MAJOR_VERSION"
endef

define VQEC_VMAJ_VAL
  sed 's/\#define VQEC_MAJOR_VERSION\s//g'
endef

define VQEC_VMIN_STR 
 grep -e "VQEC_MINOR_VERSION"
endef

define VQEC_VMIN_VAL
  sed 's/\#define VQEC_MINOR_VERSION\s//g'
endef

VER_FILE = $(ROOT)/eva/vqec_version.h

ifeq "$(findstring clean, $(MAKECMDGOALS))" ""
PIC_VER_BUILD = \
	 $(shell cat $(VER_FILE) | $(BUILD_NUMBER_STR) | $(BUILD_NUMBER_VAL))
PIC_VER_PROD = \
	 $(shell cat $(VER_FILE) | $(PROD_NUMBER_STR) | $(PROD_NUMBER_VAL))
PIC_VER_MAJOR = \
	 $(shell cat $(VER_FILE) | $(VQEC_VMAJ_STR) | $(VQEC_VMAJ_VAL))
PIC_VER_MINOR = \
	 $(shell cat $(VER_FILE) | $(VQEC_VMIN_STR) | $(VQEC_VMIN_VAL))

PIC_VERSION = $(PIC_VER_PROD).$(PIC_VER_MAJOR).$(PIC_VER_MINOR).$(PIC_VER_BUILD)
endif

pic_build:: $(LIBDIR)/libvqec_r.$(PIC_VERSION).so
pic_build:: $(LIBDIR)/libvqec_kmod_r.$(PIC_VERSION).so
pic_build:: $(MODOBJ)/test_vqec_ifclient_vqecso

$(LIBDIR)/libvqec_r.$(PIC_VERSION).so: $(VQEC_PIC_LIB) \
		$(VQEC_DEP_LIBS_R) $(VQEC_DEP_ELOG_R)
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(VQEC_DEP_EXTERNAL) \
		$(VQEC_DEP_EXTERNAL_SO)

$(LIBDIR)/libvqec_kmod_r.$(PIC_VERSION).so: $(VQEC_PIC_LIB_KMOD) \
		$(VQEC_DEP_LIBS_KMOD_R) $(VQEC_DEP_ELOG_R)
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(VQEC_DEP_EXTERNAL)

$(MODOBJ)/test_vqec_ifclient_vqecso: $(TEST_VQEC_IFCLIENT_OBJ) \
		$(LIBDIR)/libvqec_r.$(PIC_VERSION).so \
		$(VQEC_DEP_ADDONS_R)
	$(CC) $(LDFLAGS) -o $@ $^ $(VQEC_DEP_EXTERNAL) \
		-lvqec_r.$(PIC_VERSION)

#
# Sources
#
SRC = 	$(TEST_VQEC_IFCLIENT_SRC) 


# Clean build. remove libs and objs.  Define TEST_MODULE_TARGETS_FILES for
# full path to TEST_MODULE_TARGETS_FILES var.
TEST_MODULE_TARGETS_FILES = $(addprefix $(MODOBJ)/, $(TEST_MODULE_TARGETS))
clean::
	$(RM) $(TEST_MODULE_TARGETS_FILES)
	$(RM) $(MODOBJ)/test_vqec_ifclient_vqecso 
	$(RM) $(LIBDIR)/*.so

# always include depend.mk except if target does not need it included
# targets that do not need depend.mk are defined in vam.mk. Copy lines
# below as-is.
ifeq ($(include-depend-mk), 1)
include $(ROOT)/depend.mk
endif
