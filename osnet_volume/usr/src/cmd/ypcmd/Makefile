#
#ident	"@(#)Makefile	1.34	99/07/18 SMI"
#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/ypcmd/Makefile
 

BINPROG = ypmatch ypwhich ypcat
SBINPROG = makedbm ypalias ypinit yppoll ypset
NETYPPROG = ypbind ypxfr ypserv yppush ypxfrd udpublickey mkalias \
		multi multi.awk stdethers stdhosts \
		ypxfr_1perday ypxfr_1perhour ypxfr_2perday \
		ypstart ypstop
PROG=	$(BINPROG) $(SBINPROG) $(NETYPPROG)

include ../Makefile.cmd

# installed directories
NETSVC = $(ROOTLIB)/netsvc
NETYP = $(NETSVC)/yp

ROOTVAR_YP = $(ROOT)/var/yp
ROOTBINDING = $(ROOTVAR_YP)/binding
ROOTDIRS =	$(ROOT) $(ROOTUSR) $(ROOTLIB) $(ROOTETC) $(ROOTVAR) \
		$(NETSVC) $(NETYP) $(ROOTVAR_YP) $(ROOTBINDING) \
		$(ROOTUSRSBIN) $(ROOTBIN)

LDLIBS.yp = -lnsl -ldl
#LDLIBS.yp = -Bstatic -lnsl -Bdynamic -ldl -Bstatic
LDLIBS += $(LDLIBS.yp)

$(ROOTVAR_YP)/aliases :=	GROUP=bin
$(ROOTVAR_YP)/aliases :=	FILEMODE=555

YPFILES = Makefile aliases nicknames updaters
ETCFILES =  publickey
ROOTYPFILES= $(YPFILES:%=$(ROOTVAR_YP)/%)
ROOTETCFILE= $(ETCFILES:%=$(ROOTETC)/%)

TXTS=	$(ETCFILES:%=net_files/%) $(YPFILES:%=net_files/%)
SUBDIRS = yppasswd mknetid revnetgroup ypupdated

YPBINDOBJ= yp_b_svc.o yp_b_subr.o rpc_bootstrap.o getlist.o
COMMONOBJ= getlist.o yp_getalias.o
YPXFROBJ = ypxfr.o ypxfrd_client.o ypxfrd_xdr.o lockmap.o
YPMATOBJ = ypmatch.o nick.o
YPWHIOBJ = ypwhich.o nick.o
YPCATOBJ = ypcat.o nick.o
UDPUBLICKEYOBJ = udpublickey.o
MKALIASOBJ = mkalias.o
STDETHERSOBJ = stdethers.o
STDHOSTSOBJ = stdhosts.o
#COMMONCLNTOBJ =	clnt_create_rsvd.o
YPSERVOBJ = ypserv.o ypserv_ancil.o ypserv_map.o \
		ypserv_proc.o  \
		ypserv_resolv.o ypserv_resolv_common.o \
		ypv1_xdr.o ypserv_net_secure.o lockmap.o
YPPUSHOBJ = yppush.o
YPXFRDOBJ = ypxfrd_svc.o ypxfrd_xdr.o ypxfrd_server.o \
		ypserv_net_secure.o

NETYPOBJ = $(YPXFROBJ) $(YPXFRDOBJ) $(YPSERVOBJ) $(YPPUSHOBJ) \
		$(YPBINDOBJ) $(UDPUBLICKEYOBJ)
OBJS = $(NETYPOBJ) $(MKALIASOBJ) $(COMMONOBJ) \
	$(YPPUSHOBJ) $(YPMATOBJ) $(YPWHIOBJ) $(YPCATOBJ) \
	$(STDETHERSOBJ) $(STDHOSTSOBJ) makedbm.o ypalias.o pong.o

BINSRC = $(BINPROG:=.c)

SBINSRC = $(SBINPROG:=.c)

NETYPSRC = $(NETYPOBJ:.o=.c)

YPBINDSRC = $(YPBINSOBJ:.o=.c)

YPXFRSRC =	$(YPXFROBJ:.o=.c)

YPSERVSRC =	$(YPSERVOBJ:.o=.c)

YPPUSHSRC =	$(YPPUSHOBJ:.o=.c)

YPXFRDSRC =	$(YPXFRDOBJ:.o=.c)

YPMASTERSRC =	$(YPMASTEROBJ:.o=.c)

YPALLSRC =	$(YPALLOBJ:.o=.c)

COMMONSRC = 	$(COMMONOBJ:.o=.c)

#COMMONCLNTSRC =	$(COMMONCLNTOBJ:.o=.c)

SRCS = $(BINSRC) $(SBINSRC) $(NETYPSRC) \
	$(COMMONSRC)

CLEANFILES =	ypxfrd.h ypxfrd_xdr.c ypxfrd_svc.c

IBINPROG=		$(BINPROG:%=$(ROOTBIN)/%)
ISBINPROG=		$(SBINPROG:%=$(ROOTUSRSBIN)/%)
INETYPPROG=		$(NETYPPROG:%=$(NETYP)/%)

#conditional assignments
ypalias.o :=	CPPFLAGS= -DMAIN $(CPPFLAGS.master)
makedbm.o :=	CPPFLAGS= $(CPPFLAGS.master)
yp_b_svc.o :=	CPPFLAGS += -DINIT_DEFAULT
ypxfrd_svc.o := CPPFLAGS += -Dmain=_main
$(IBINPROG) := GROUP= other
$(ROOTVAR_YP)/Makefile := FILEMODE= 555
$(ROOTVAR_YP)/nicknames := FILEMODE= 644
$(ROOTVAR_YP)/updaters := FILEMODE= 500
$(ROOTETC)/publickey := FILEMODE= 644
# non-default file attributes to avoid conflict with rpcsrc
$(NETSVC) :=	GROUP=          sys
NETYP =		$(NETSVC)/yp
$(NETYP) :=	GROUP=bin


all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint
cstyle :=	TARGET= cstyle

all: $(PROG) $(TXTS) $(SUBDIRS)

# install rules
$(ROOTVAR_YP)/% \
$(ROOTETC)/%: net_files/%
	$(INS.file)

$(ROOTVAR_YP)/% : net_files/%
	$(INS.file)

$(NETYP)/%: %
	$(INS.file)

.KEEP_STATE:

ypbind: $(YPBINDOBJ)
	$(LINK.c) $(YPBINDOBJ) -o $@ $(LDLIBS)
	$(POST_PROCESS)

makedbm ypalias: $$@.o $(COMMONOBJ)
	$(LINK.c) -o $@ $@.o $(COMMONOBJ) $(LDLIBS)
	$(POST_PROCESS)

ypmatch: $(YPMATOBJ)
	$(LINK.c) -o $@ $(YPMATOBJ) $(LDLIBS)
	$(POST_PROCESS)

ypwhich: $(YPWHIOBJ)
	$(LINK.c) -o $@ $(YPWHIOBJ) $(LDLIBS)
	$(POST_PROCESS)

ypcat: $(YPCATOBJ)
	$(LINK.c) -o $@ $(YPCATOBJ) $(LDLIBS)
	$(POST_PROCESS)

ypxfrd_xdr.c: ypxfrd.x
	$(RM) ypxfrd_xdr.c; $(RPCGEN) -C -c ypxfrd.x -o ypxfrd_xdr.c

ypxfrd.h: ypxfrd.x
	$(RM) ypxfrd.h; $(RPCGEN) -C -h ypxfrd.x -o ypxfrd.h

ypxfrd_xdr.o ypxfrd_client.o: ypxfrd.h

#clnt_create_rsvd.o ypxfr.o: clnt_create_rsvd.h


ypxfrd_svc.c: ypxfrd.x
	$(RM) ypxfrd_svc.c; $(RPCGEN) -C -s visible ypxfrd.x -o ypxfrd_svc.c

ypxfrd: $(YPXFRDOBJ)
	$(LINK.c) $(YPXFRDOBJ) -o $@ $(LDLIBS)
	$(POST_PROCESS)

ypxfr: $(YPXFROBJ) $(COMMONOBJ) $(YPMASTEROBJ)
	$(LINK.c) -o $@ $(YPXFROBJ) $(YPMASTEROBJ) $(LDLIBS)
	$(POST_PROCESS)

ypserv: $(YPSERVOBJ) $(COMMONOBJ)
	$(LINK.c) $(YPSERVOBJ) $(COMMONOBJ) -o $@ $(LDLIBS)
	$(POST_PROCESS)

yppush: $(YPPUSHOBJ) $(COMMONOBJ)
	$(LINK.c) $(YPPUSHOBJ) $(COMMONOBJ) -o $@ $(LDLIBS)
	$(POST_PROCESS)

udpublickey: $(UDPUBLICKEYOBJ)
	$(LINK.c) $(UDPUBLICKEYOBJ) -o $@
	$(POST_PROCESS)

mkalias: $(MKALIASOBJ)
	$(LINK.c) $(MKALIASOBJ) -o $@ -lnsl
	$(POST_PROCESS)

stdethers: $(STDETHERSOBJ)
	$(LINK.c) $(STDETHERSOBJ) -o $@ -lsocket -lnsl
	$(POST_PROCESS)

stdhosts: $(STDHOSTSOBJ)
	$(LINK.c) $(STDHOSTSOBJ) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(PROG) $(ROOTDIRS) $(ROOTETCFILE) $(ROOTYPFILES) $(IBINPROG) \
	$(ISBINPROG) $(INETYPPROG) $(SUBDIRS)

$(ROOTDIRS):
	$(INS.dir)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

#lint := LINTFLAGS=-muvx
lint := LINTFLAGS=-x

lint:	$(SUBDIRS)
	${LINT.c} `echo $(SRCS) | sed 's/ypinit.c//'`

cstyle:	$(SUBDIRS)
	${CSTYLE} `echo $(SRCS) | sed 's/ypinit.c//'`

clean:	$(SUBDIRS)
	$(RM) $(OBJS) $(CLEANFILES)

clobber: clean $(SUBDIRS)
	$(RM) $(PROG)

cscope.files: FRC
	@echo "Building cscope.files"
	@ls `echo  $(SRCS) ypxfrd.x Makefile | sed 's/ypinit.c/ypinit.sh/'` > cscope.files

cscope.out: cscope.files
	@cscope -b

FRC:
