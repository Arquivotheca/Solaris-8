#
#ident	"@(#)Makefile	1.25	98/01/28 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/rexd/Makefile

PROG=		rpc.rexd on
# where under
#RPCGEN=		rpcgen
REXDPROG=	rpc.rexd
ONPROG	=	on

include ../Makefile.cmd

SRCS=		$(PROG:%=%.c)

#OBJS=		errprintf.o mount_nfs.o rex_xdr.o unix_login.o mntent.o
OBJS=		errprintf.o mount_nfs.o rex.o unix_login.o mntent.o
REXDOBJS=	rpc.rexd.o errprintf.o mount_nfs.o rex.o unix_login.o mntent.o
ONOBJS= 	on.o sharetab.o where.o rex.o mntent.o
#WHEREOBJS=	sharetab.o where.o where_main.o mntent.o
CLEANOBJS=	exportent.o sharetab.o on.o rpc.rexd.o where.o
#under.o where.o where_main.o
#UNDEROBJS=	errprintf.o sharetab.o mount_nfs.o

ROOTREXD=	$(ROOTUSRSBIN)
ROOTON=		$(ROOTBIN)
ROOTREXDPROG=	$(REXDPROG:%=$(ROOTREXD)/%)
ROOTONPROG=	$(ONPROG:%=$(ROOTON)/%)

CPPFLAGS =	-I. -I$(ROOT)/usr/include/rpcsvc $(CPPFLAGS.master)
CPPFLAGS =	-I. $(CPPFLAGS.master)

LDLIBS +=	 -lsocket -lrpcsvc -lnsl -lbsm -lpam -ldl

# install rule
# don't re-install directories created by Targetdirs
#$(ROOTREXD)/%: %
#	$(INS.file)
#$(ROOTON)/%: %
#	$(INS.file)

.KEEP_STATE:

all:	$(PROG)

rpc.rexd:	$$@.o $(OBJS)
	$(LINK.c) -o $@ $@.o $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

on:	$(ONOBJS)
	$(LINK.c) -o $@ $(ONOBJS) $(LDLIBS)
	$(POST_PROCESS)

#under: $$@.o $(UNDEROBJS)
#	$(LINK.c) -o $@ $@.o $(UNDEROBJS) $(LDLIBS)
#
#where: $(WHEREOBJS)
#	$(LINK.c) -o $@ $(WHEREOBJS) $(LDLIBS)
#
install: all $(ROOTREXDPROG) $(ROOTONPROG)

$(ROOTREXD):	rpc.rexd
	$(INS.dir)
$(ROOTON):	on
	$(INS.dir)

clean:
	$(RM) $(OBJS) $(CLEANOBJS) $(PROG)

lint:	 lint_SRCS

include ../Makefile.targ

# derived source files and explicit dependencies

#rex_xdr.o \
#rex_svc.o \
#rex_clnt.o:	rex.h rex.c
#	$(LINK.c) -c -o $@ rex.c

on.o:		rex.h

rpc.rexd.o:		rex.h

unix_login.o:	rex.h

rex.o:		rex.h
