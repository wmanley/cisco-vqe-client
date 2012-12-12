#
# Copyright (c) 2006-2008 by Cisco Systems, Inc.
# All Rights Reserved.
#

# local include paths go here
INCLUDES +=  -I$(ROOT)/include/utils

# libraries you want to link against - they could be anywhere in the search path,
# or in the $(LIBDIR) dir. $(ROOT) is the vam comp dir.
LIBS += -L$(LIBDIR) -lzonemgr -lidmgr
#
# Build a local library for others to use!
#

# library source - should be in this dir
LIB_SRC = $(SRCDIR)/zone_mgr.c $(SRCDIR)/vam_debug.c $(SRCDIR)/vam_time.c \
          $(SRCDIR)/shm_api.c $(SRCDIR)/vam_ipc.c $(SRCDIR)/vam_hist.c \
          $(SRCDIR)/vam_util.c $(SRCDIR)/vmd5.c $(SRCDIR)/strlcpy.c \
	  $(SRCDIR)/strlcat.c $(SRCDIR)/vqe_hash.c $(SRCDIR)/vqe_bitmap.c \
          $(SRCDIR)/vqe_token_bucket.c $(SRCDIR)/raw_packet.c \
          $(SRCDIR)/raw_socket.c $(SRCDIR)/socket_filter.c

# name of the execs to build, copy build, PHONY rules, prog-name rules
PROG = vam_debug vam_prog_1 vam_time_test vam_hist_test id_manager_test \
	vam_util_test

ifeq ($(ARCH),x86)
  ifeq "$(findstring vqec, $(BUILD_PRODUCT))" ""
    LIB_SRC += $(SRCDIR)/vqe_caps.c $(SRCDIR)/vqe_pcap.c \
	       $(SRCDIR)/vqes_fbt_addr.c
    PROG += fbt_addr_test
  endif
endif

build:: $(PROG)
.PHONY: $(PROG)


# name of library (gets put into the lib dir)
LIB = $(LIBDIR)/libzonemgr.a
# copy the section below as is
LIB_OBJ = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(LIB_SRC:.c=.o))
LIB_MEMBERS = $(LIB)($(LIB_OBJ))
libs:: make_ver $(LIB) update_ver
$(LIB): $(LIB_MEMBERS)
$(LIB)():
	$(AR) crv $(LIB) $^
$(LIB)(%o): $(MODOBJ)/%.o
	$(AR) crv $@ $^


####
# Build PIC library
####
PIC_LIB_UTIL = $(patsubst %.a, %_r.o, $(LIB))
$(PIC_LIB_UTIL): $(LIB_OBJ)
	$(LD) -r -o $@ $^

pic_libs:: $(PIC_LIB_UTIL)

# build idmgr.a
LIB_IDMGR_SRC = $(SRCDIR)/id_manager.c
# name of library (gets put into the lib dir)
LIB_OBJ = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(LIB_IDMGR_SRC:.c=.o))
LIB = $(LIBDIR)/libidmgr.a
libs:: $(LIB)
$(LIB): $(LIB_OBJ)
	$(AR) crv $@ $^

####
# Build PIC library
####
PIC_LIB_ID = $(patsubst %.a, %_r.o, $(LIB))
$(PIC_LIB_ID): $(LIB_OBJ)
	$(LD) -r -o $@ $^

pic_libs:: $(PIC_LIB_ID)

#
# Build some executable (primary) - will go into obj/($DIR)
#

# sources for the exec
ZONE_MGR_TEST_SRC = $(SRCDIR)/zone_mgr_test.c
ZONE_MGR_TEST_OBJ = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(ZONE_MGR_TEST_SRC:.c=.o))

VAM_DEBUG_TEST_SRC = $(SRCDIR)/vam_debug_test.c
VAM_DEBUG_TEST_OBJ = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(VAM_DEBUG_TEST_SRC:.c=.o))


vam_debug: $(MODOBJ)/vam_debug
vam_prog_1: $(MODOBJ)/vam_prog_1

$(MODOBJ)/vam_debug: $(ZONE_MGR_TEST_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lstdc++ -lpthread $(LIBS)

$(MODOBJ)/vam_prog_1: $(VAM_DEBUG_TEST_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lstdc++ -lpthread $(LIBS)

VAM_TIME_TEST_SRC = $(SRCDIR)/vam_time_test.c $(SRCDIR)/vam_time.c
VAM_TIME_TEST_OBJS = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(VAM_TIME_TEST_SRC:.c=.o))
vam_time_test: $(MODOBJ)/vam_time_test
$(MODOBJ)/vam_time_test: $(VAM_TIME_TEST_OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) $^ -o $@ $(LIBS)

VAM_HIST_TEST_SRC = $(SRCDIR)/vam_hist_test.c $(SRCDIR)/vam_hist.c
VAM_HIST_TEST_OBJS = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(VAM_HIST_TEST_SRC:.c=.o))
vam_hist_test: $(MODOBJ)/vam_hist_test
$(MODOBJ)/vam_hist_test: $(VAM_HIST_TEST_OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) $^ -o $@ $(LIBS)

ID_MGR_TEST_SRC = $(SRCDIR)/id_manager_test.c
ID_MGR_TEST_OBJS = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(ID_MGR_TEST_SRC:.c=.o))
id_manager_test: $(MODOBJ)/id_manager_test
$(MODOBJ)/id_manager_test: $(ID_MGR_TEST_OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) $^ -o $@ $(LIBS)

VAM_UTIL_TEST_SRC = $(SRCDIR)/vam_util_test.c
VAM_UTIL_TEST_OBJS = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(VAM_UTIL_TEST_SRC:.c=.o))
vam_util_test: $(MODOBJ)/vam_util_test
$(MODOBJ)/vam_util_test: $(VAM_UTIL_TEST_OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) $^ -o $@ $(LIBS)


FBT_ADDR_TEST_SRC = $(SRCDIR)/fbt_addr_test.c
FBT_ADDR_TEST_OBJ = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(FBT_ADDR_TEST_SRC:.c=.o))
fbt_addr_test: $(MODOBJ)/fbt_addr_test
$(MODOBJ)/fbt_addr_test: $(FBT_ADDR_TEST_OBJ) $(LIB)
	$(CC) $(LDFLAGS) $(CFLAGS) $^ -o $@ $(LIBS) $(LIBDIR)/libdebug.a -lpthread -lcap


#
# Build utrun test programs
#
# Currently this is just the token bucket tests.
#
utrun_build:: test_vqe_token_bucket
utrun:: utrun_libs utrun_build
	@echo "running utils unit tests"
	$(MODOBJ)/test_vqe_token_bucket
	$(LCOV_DIR)/lcov --capture --directory $(MODOBJ) -base-directory . --output-file $(MODOBJ)/utest.info --test-name utest
	$(LCOV_DIR)/genhtml $(MODOBJ)/utest.info --output-directory $(MODOBJ)/output --title "utils unit tests" --show-details --legend

#
# define utrun test objects
#
# These are the object files corresponding to the sources, plus the
# libcunit.a library
#
TEST_VQE_TOKEN_BUCKET_UTEST_OBJS = $(patsubst $(SRCDIR)/%, $(MODOBJ)/%, $(TEST_VQE_TOKEN_BUCKET_UTEST_SRC:.c=.o))
TEST_VQE_TOKEN_BUCKET_UTEST_OBJS += $(ROOT)/add-ons/lib/$(ARCH)/libcunit.a 
TEST_VQE_TOKEN_BUCKET_UTEST_OBJS += $(LIBDIR)/libzonemgr.a

#
# define utrun test sources
# This includes the token bucket test files plus implementation file
#
TEST_VQE_TOKEN_BUCKET_UTEST_SRC = $(SRCDIR)/vqe_token_bucket_testmain.c \
                                  $(SRCDIR)/vqe_token_bucket_tests.c \
                                  $(SRCDIR)/vqe_token_bucket.c

#
# define how to build the utrun test program
#
test_vqe_token_bucket: $(MODOBJ)/test_vqe_token_bucket
$(MODOBJ)/test_vqe_token_bucket: $(TEST_VQE_TOKEN_BUCKET_UTEST_OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) $(INCLUDES) -o $@ $^

#
# Build another (secondary) executable - will go into obj/($DIR)
#



# relink main program
relink:
	$(RMDIR) $(MODOBJ)/vam_debug
	$(MAKE) vam_debug


# clean build. Remove libs and exes. Define PROG_FILES for full path to PROG var.
PROG_FILES = $(addprefix $(MODOBJ)/, $(PROG))
clean::
	$(RM) $(LIBDIR)/libzonemgr.a
	$(RM) $(LIBDIR)/libidmgr.a
	$(RM) $(PROG_FILES)
	$(RM) $(SRCDIR)/update_ver

pic_clean::
	$(RM) $(LIBDIR)/libzonemgr_r.o $(LIBDIR)/libidmgr_r.o 


SRC = $(LIB_SRC) \
      $(LIB_IDMGR_SRC) \
      $(ZONE_MGR_TEST_SRC) \
      $(VAM_DEBUG_TEST_SRC) \
      $(VAM_TIME_TEST_SRC) \
      $(VAM_HIST_TEST_SRC) \
      $(ID_MGR_TEST_SRC) \
      $(VAM_UTIL_TEST_SRC)


update_ver: $(LIB)
	@cd $(SRCDIR); \
	./update_version.sh $(VER_BUILD_TYPE); \
	echo "updated vqes_version.h"; \
	touch update_ver;

make_ver:
	@cd $(SRCDIR); \
	if [ ! -e vqes_version.h ]; then \
		./update_version.sh $(VER_BUILD_TYPE); \
		echo "created vqes_version.h"; \
	fi;
 
# always include depend.mk except if target does not need it included
# targets that do not need depend.mk are defined in vam.mk. Copy lines
# below as-is.
ifeq ($(include-depend-mk), 1)
include $(ROOT)/depend.mk
endif

