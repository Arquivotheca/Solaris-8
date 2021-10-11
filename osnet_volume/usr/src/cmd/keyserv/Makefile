#
# Copyright (c) 1988-1997,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.28	99/12/06 SMI"
#
# cmd/keyserv/Makefile

SBINPROG = keyserv newkey
BINPROG	= keylogout keylogin domainname chkey

PROG= $(SBINPROG) $(BINPROG)

HDRS= key_prot.h

KEYSERVOBJS = keyserv.o
KEYLOGOUTOBJS = keylogout.o
KEYLOGINOBJS = keylogin.o
CHKEYOBJS = chkey.o
NEWKEYOBJS = newkey.o
UPDATEOBJS = update.o
DOMAINNAMEOBJS = domainname.o
K_OBJS = setkey.o detach.o key_generic.o keyserv_cache.o
CHANGE_OBJS  = update.o chkey_common.o
OUTSIDE_UTIL_OBJS = selfcheck.o

OBJS	= $(KEYSERVOBJS) $(KEYLOGOUTOBJS) $(KEYLOGINOBJS) $(CHKEYOBJS) \
	$(NEWKEYOBJS) $(UPDATEOBJS) $(DOMAINNAMEOBJS) \
	$(K_OBJS)  $(CHANGE_OBJS)
SRCS	= $(OBJS:.o=.c)

SED=	sed

include ../Makefile.cmd

IBINPROG=		$(BINPROG:%=$(ROOTBIN)/%)
ISBINPROG=		$(SBINPROG:%=$(ROOTUSRSBIN)/%)

$(ROOTUSRSBIN)/keyserv \
$(ROOTUSRSBIN)/newkey :=			OWNER=		root
$(ROOTUSRSBIN)/keyserv \
$(ROOTUSRSBIN)/newkey :=			GROUP=		sys

$(ROOTBIN)/chkey :=				OWNER=		root
$(ROOTBIN)/chkey :=				GROUP=		sys
$(ROOTBIN)/chkey :=				FILEMODE=	4555

CPPFLAGS += -I. -I$(SRC)/lib/libnsl/include -I$(SRC)/lib/libsldap/common \
		-D_REENTRANT

LDLIBS += -lrpcsvc -lnsl -lmp -lsldap
keyserv := LDLIBS += -lthread
newkey  := LDLIBS += -lsocket


.KEEP_STATE:

all: $(PROG)

keyserv: $(K_OBJS)  $(KEYSERVOBJS)
	$(LINK.c) $(K_OBJS)  $(KEYSERVOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

keylogout: $(KEYLOGOUTOBJS)
	$(LINK.c) $(KEYLOGOUTOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

keylogin: $(KEYLOGINOBJS)
	$(LINK.c) $(KEYLOGINOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

chkey: $(CHKEYOBJS) $(CHANGE_OBJS)
	$(LINK.c) $(CHANGE_OBJS) $(CHKEYOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

newkey:$(CHANGE_OBJS)  $(NEWKEYOBJS) $(OUTSIDE_UTIL_OBJS)
	$(LINK.c) $(CHANGE_OBJS)  $(NEWKEYOBJS) $(OUTSIDE_UTIL_OBJS) \
	-o $@ $(LDLIBS)
	$(POST_PROCESS)

update: $(UPDATEOBJS)
	$(LINK.c)  $(UPDATEOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

domainname: $(DOMAINNAMEOBJS)
	$(LINK.c) $(DOMAINNAMEOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

selfcheck.o: ../fs.d/nfs/lib/selfcheck.c
	$(COMPILE.c) ../fs.d/nfs/lib/selfcheck.c

install: all $(DIRS) $(IBINPROG) $(ISBINPROG)

clean:
	$(RM) $(OBJS)

lint: lint_SRCS

key_prot.h : $(ROOT)/usr/include/rpcsvc/key_prot.x
	$(RPCGEN) -h $(ROOT)/usr/include/rpcsvc/key_prot.x > key_prot.h

include ../Makefile.targ

