#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile.com	1.9	99/01/25 SMI"
#
# lib/librpcsvc/Makefile
#

LIBRARY= librpcsvc.a
VERS = .1

OBJECTS= rstat_simple.o rstat_xdr.o rusers_simple.o rusersxdr.o rusers_xdr.o \
	 rwallxdr.o spray_xdr.o nlm_prot.o sm_inter_xdr.o nsm_addr_xdr.o \
	 bootparam_prot_xdr.o mount_xdr.o mountlist_xdr.o rpc_sztypes.o \
	 bindresvport.o

# include library definitions
include ../../Makefile.lib

# Don't mess with this. DAAMAPFILE gets correctly overridden
# for 64bit.
MAPFILE=	$(MAPDIR)/mapfile

CLOBBERFILES +=	$(MAPFILE)

SRCS=		$(OBJECTS:%.o=../common/%.c)

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

LIBS += $(DYNLIB)


CPPFLAGS += -DYP
LDLIBS += -lnsl -lc
DYNFLAGS += -M $(MAPFILE)

.KEEP_STATE:


$(DYNLIB): $(MAPFILE)
$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

# include library targets
include ../../Makefile.targ
