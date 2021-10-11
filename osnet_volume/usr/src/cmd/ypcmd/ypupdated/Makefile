#
#ident	"@(#)Makefile	1.2	99/03/30 SMI"
#
# Copyright (c) 1989,1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/ypcmd/ypupdated/Makefile

NETYPPROG =	rpc.ypupdated
PROG =		$(NETYPPROG)

include ../../Makefile.cmd

#installed directories
RPCSVC=		$(ROOT)/usr/include/rpcsvc
NETSVC =	$(ROOTLIB)/netsvc
NETYP =		$(NETSVC)/yp
ROOTDIRS =	$(NETSVC) $(NETYP)

# include library definitions
#LDLIBS +=	-lrpcsvc -lnsl -lcrypt -lintl -lgen
#LDLIBS +=	-lnsl -lcrypt
LDLIBS +=	-lrpcsvc -lnsl -ldl

# This file is now in the $(SRC)/head/rpcsvc directory.
#HDRFILE= yppasswd.h
#IHDRFILE= $(HDRFILE:%=$(RPCSVC)/%)

INETYPPROG=		$(NETYPPROG:%=$(NETYP)/%)

RPCYPUPDATEOBJ =	rpc.ypupdated.o openchild.o

OBJS =			$(RPCYPUPDATEOBJ)

SRCS =			$(OBJS:%.o=%.c)

#conditional assignments
$(INETSVC)   := GROUP=bin
$(INETSVC)   := FILEMODE=555

#install rules

.KEEP_STATE:

all: $(PROG)

ypupdated_prot.h: ypupdate_prot.x
	$(RM) ypupdated_prot.h; $(RPCGEN) -C -h ypupdate_prot.x -o ypupdated_prot.h

$(RPCYPUPDATEOBJ): ypupdated_prot.h

rpc.ypupdated: $(RPCYPUPDATEOBJ)
	$(LINK.c) -o $@ $(RPCYPUPDATEOBJ) $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTDIRS) $(IBINPROG) $(INETYPPROG)

$(ROOTDIRS):
	$(INS.dir)

$(NETYP)/%: %
	$(INS.file)

clean:
	$(RM) $(OBJS)

lint:
	${LINT.c} ${SRCS}

cstyle:
	${CSTYLE} ${SRCS}

# include library targets
include ../../Makefile.targ
