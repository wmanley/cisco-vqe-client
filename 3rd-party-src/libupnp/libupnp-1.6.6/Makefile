
include ../../../environ.mk

.PHONY: all threadutil ixml upnp

all: 
	$(MAKE) libupnp.a
	$(MAKE) libupnp.so

libupnp.a: threadutil ixml upnp
	$(AR) rcs libupnp.a *.o
	rm -f *.o

libupnp.so: PIC_FLAGS = -fPIC
libupnp.so: threadutil ixml upnp
	$(CC) -shared -Wl,-soname,libupnp.so -o libupnp.so *.o
	ln -sf libupnp.so libupnp.so.3
	rm -f *.o

ifneq ($(ARCH), x86)
    UPNP_FLAGS += -D_XOPEN_SOURCE=500
endif

threadutil: 
	$(CC) -g -c $(UPNP_FLAGS) $(PIC_FLAGS) threadutil/src/*.c -I. -Ithreadutil/inc/

ixml: 
	$(CC) -c -g $(UPNP_FLAGS) $(PIC_FLAGS) ixml/src/*.c -I. -Iixml/inc/ -Iixml/src/inc/

upnp: 
	$(CC) -g -c $(UPNP_FLAGS) $(PIC_FLAGS) -Iupnp/inc/ -Iupnp/src/inc/ -Iixml/inc/ -Ithreadutil/inc/ -I. upnp/src/*.c upnp/src/[^a]*/*.c upnp/src/*/*/*.c upnp/src/*/*/*/*.c upnp/src/api/upnpapi.c

clean:
	rm -f *.o
	rm -f *.a
	rm -f *.so



