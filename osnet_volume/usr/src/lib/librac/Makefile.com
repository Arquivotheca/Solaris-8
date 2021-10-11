#
#ident	"@(#)Makefile.com	1.3	99/01/25 SMI"
#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/librac/Makefile
#
LIBRARY= librac.a
VERS = .1

# objects are listed by source directory

RPC=  \
clnt_generic.o clnt_dg.o rac.o clnt_vc.o  rpcb_clnt.o xdr_rec_subr.o xdr_rec.o


OBJECTS= $(RPC)

# librac build rules

objs/%.o profs/%.o pics/%.o: ../rpc/%.c
	$(COMPILE.c) -DPORTMAP -DNIS  -o $@  $<
	$(POST_PROCESS_O)

# include library definitions
include ../../Makefile.lib

MAPFILE=	$(MAPDIR)/mapfile
CLOBBERFILES +=	$(MAPFILE)

LIBS += $(DYNLIB)

# definitions for lint

$(LINTLIB):= SRCS=../rpc/llib-lrac
$(LINTLIB):= LINTFLAGS=-nvx

LINTOUT=	lint.out

LINTSRC=	$(LINTLIB:%.ln=%)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)
CLEANFILES +=	$(LINTOUT) $(LINTLIB)

LDLIBS += -lnsl -ldl -lc
DYNFLAGS += -M $(MAPFILE)
#CFLAGS += -DPRINTFS
#CFLAGS64 += -DPRINTFS

.KEEP_STATE:

$(DYNLIB):	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

lint:		$(LINTLIB)

include ../../Makefile.targ

# install rule for lint library target
$(ROOTLINTDIR)/%:	../rpc/%
	$(INS.file)
