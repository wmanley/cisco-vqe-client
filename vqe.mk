###############################################################################
# Configuration file for all VQE makefiles. 
# Only global definitions here. No BUILD specific info should be added here.
# build specific info should be in vqe_builds.mk. This mk file is included in
# both source and object directory makefiles so only only define vars here 
# that will be needed by both makefiles, example MAKE_K_CHECK which is used
# by all makes and is needed in both source and object makefiles. Also define 
# vars here that will be needed by both vqe_targets.mk and vqe_build.mk
#
# Copyright (c) 2006-2007 by Cisco Systems, Inc.
# All rights reserved. 
###############################################################################

# Global Build variables. These will set what product is being built.
MAKE=		/usr/bin/make
ROOT=		$(shell until [ -f vqe.mk ]; do cd ..; done; /bin/pwd)
SRCMODULE:=         $(subst $(ROOT),,$(shell /bin/pwd))
PATH:=		/usr/bin:$(PATH)
ACME=		/usr/cisco/bin/acme     
MKID=           /router/bin/mkid32
ETAGS=          /router/bin/etags

# define build check variables here, which is common for all products.
MAKE_K_CHECK = \
	if ! echo "$(MAKEFLAGS)" | grep '^[^ ]*k' > /dev/null; then \
	exit 1; \
fi \
