#
#ident   "@(#)Makefile	1.9	98/04/16 SMI"
#
# Copyright (c) 1994, 1996, 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/common/rpcsvc/Makefile
#	This makefile installs system header files that go into
#	/usr/include/rpcsvc.
#
# include global definitions
include ../../../Makefile.master

# Protocol descriptions.  Alas, the NFS protocol cannot be expressed
# completely via rpcgen.  The NLM description should go here some day.
RPCGEN_SRC=	autofs_prot.x sm_inter.x nfsauth_prot.x nsm_addr.x

DERIVED_HDRS=	$(RPCGEN_SRC:%.x=%.h)

ALLHDRS=	$(RPCGEN_SRC) $(DERIVED_HDRS)

ROOTDIRS=	$(ROOT)/usr/include/rpcsvc

ROOTHDRS=	$(ALLHDRS:%=$(ROOTDIRS)/%)

$(ROOTDIRS)/%: %
	$(INS.file)

.KEEP_STATE:

# all_h permits derived headers to be built here in the uts source area
# for the kernel to reference, without going so far as to install them.
#
all_h: $(DERIVED_HDRS)

install_h: all_h $(ROOTDIRS) $(ROOTHDRS)

clean:
	$(RM) $(DERIVED_HDRS)

clobber: clean

# Don't check rpcgen-derived files.
check:

$(ROOTDIRS):
	$(INS.dir)

%.h: %.x
	$(RPCGEN) -C -h $< -o $@

