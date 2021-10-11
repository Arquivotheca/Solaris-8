#
# Copyright (c) 1992 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.25	97/04/27 SMI"
#
# cmd/rpcsvc/Makefile

PROTOCOL_DIR= $(ROOT)/usr/include/rpcsvc

BINPROG= rusers rup
SBINPROG= rwall spray
RPROG= rpc.rusersd
SPROG= rpc.sprayd
WPROG= rpc.rwalld
TPROG= rpc.rstatd

PROG= $(BINPROG) $(SBINPROG) $(RPROG) $(SPROG) $(WPROG) $(TPROG)

SCLNTOBJS= spray.o spray_clnt.o
SSVCOBJS= spray_subr.o spray_svc.o
WCLNTOBJS = rwall.o rwall_clnt.o
WSVCOBJS= rwall_subr.o rwall_svc.o
TSVCOBJS= rstat_main.o rstat_proc.o rstat_svc.o rstat_v2_svc.o rstat_v2_xdr.o

DERIVED_FILES = rstat.x rstat.h rstat_svc.c rstat_v2.h rstat_v2_svc.c \
		rstat_v2_xdr.c spray.x spray.h spray_clnt.c spray_svc.c \
		rwall.x rwall.h rwall_svc.c rwall_clnt.c

include ../Makefile.cmd

CPPFLAGS= -I. $(CPPFLAGS.master)
LDLIBS += -lnsl -lrpcsvc

CP=	cp

SUBDIRS= rpc.bootparamd nis/utils nis/bin nis/cachemgr nis/rpc.nisd \
	 nis/rpc.nispasswdd
OBJS= $(SCLNTOBJS) $(SSVCOBJS) $(WCLNTOBJS) $(WSVCOBJS) $(TSVCOBJS)
SRCS= $(OBJS:%.o=%.c) rusers.c rpc.rusersd.c rup.c

ETCFILES= rpc

TXTS= $(ETCFILES:%=net_files/%)

NETSVC=	$(ROOTLIB)/netsvc
NIS = $(NETSVC)/nis
RWALL= $(NETSVC)/rwall
RUSERS= $(NETSVC)/rusers
SPRAY= $(NETSVC)/spray
RSTAT= $(NETSVC)/rstat
ROOTVAR_NIS= $(ROOT)/var/nis

ROOTDIRS= \
		$(NETSVC) \
		$(NIS) \
		$(RWALL) \
		$(RUSERS) \
		$(SPRAY) \
		$(RSTAT) \
		$(ROOTVAR_NIS)

IBINPROG=		$(BINPROG:%=$(ROOTBIN)/%)
ISBINPROG=		$(SBINPROG:%=$(ROOTUSRSBIN)/%)
IWPROG=			$(WPROG:%=$(RWALL)/%)
ISPROG=			$(SPROG:%=$(SPRAY)/%)
IRPROG=			$(RPROG:%=$(RUSERS)/%)
ITPROG=			$(TPROG:%=$(RSTAT)/%)
IETCFILES=		$(ETCFILES:%=$(ROOTETC)/%)

# non-default file attributes
$(ROOTDIRS) :=					OWNER=		root
$(ROOTDIRS) :=					GROUP=		sys

$(ROOTETC)/rpc :=				FILEMODE=	0644
$(ROOTETC)/rpc :=				OWNER=		root
$(ROOTETC)/rpc :=				GROUP=		sys

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint

rpc.rstatd:=	LDLIBS += -lkstat
rwall:=		LDLIBS += -lthread
rpc.rwalld:=	LDLIBS += -lthread
rwall:=		CPPFLAGS += -D_REENTRANT
rpc.rwalld:=	CPPFLAGS += -D_REENTRANT

# install rules

$(RWALL)/% $(SPRAY)/% $(RUSERS)/% $(RSTAT)/%: %
	$(INS.file)

$(ROOTETC)/%: ./net_files/%
	$(INS.file)

.KEEP_STATE:

all: $(PROG) $(TXTS) $(SUBDIRS)

# multi-object targets
#
spray:	$(SCLNTOBJS)
	$(LINK.c) $(SCLNTOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

spray_svc.o spray_clnt.o: spray.h

rpc.sprayd: $(SSVCOBJS)
	$(LINK.c) $(SSVCOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

rwall_svc.o rwall_clnt.o: rwall.h

rwall: $(WCLNTOBJS)
	$(LINK.c) $(WCLNTOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

rpc.rwalld: $(WSVCOBJS)
	$(LINK.c) $(WSVCOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

rstat_main.o:	rstat.h rstat_v2.h

rstat_svc.o: rstat.h

rstat_V2_svc.o rstat_V2_xdr.o: rstat_v2.h

rpc.rstatd: $(TSVCOBJS)
	$(LINK.c) $(TSVCOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: $(PROG) $(TXTS) .WAIT $(ROOTDIRS) .WAIT $(IBINPROG) $(ISBINPROG) \
	$(IWPROG) $(ISPROG) $(IRPROG) $(ITPROG) $(IETCFILES) $(SUBDIRS)

$(ROOTDIRS):
	$(INS.dir)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

#
# Derived files
#

rstat.x: $(PROTOCOL_DIR)/rstat.x
	$(RM) rstat.x
	$(CP) $(PROTOCOL_DIR)/rstat.x rstat.x

rstat.h: $(PROTOCOL_DIR)/rstat.h
	$(RM) rstat.h
	$(CP) $(PROTOCOL_DIR)/rstat.h rstat.h

rstat_svc.c: rstat.x
	$(RPCGEN) -C -m rstat.x > $@

rstat_v2.h: rstat_v2.x
	$(RPCGEN) -C -h rstat_v2.x > $@

rstat_v2_svc.c: rstat_v2.x
	$(RPCGEN) -C -m rstat_v2.x > $@

rstat_v2_xdr.c: rstat_v2.x
	$(RPCGEN) -c rstat_v2.x > $@

spray.x: $(PROTOCOL_DIR)/spray.x
	$(RM) spray.x
	$(CP) $(PROTOCOL_DIR)/spray.x spray.x

spray.h: $(PROTOCOL_DIR)/spray.h
	$(RM) spray.h
	$(CP) $(PROTOCOL_DIR)/spray.h spray.h

spray_svc.c: spray.x
	$(RPCGEN) -s datagram_v -s circuit_v spray.x > $@

spray_clnt.c: spray.x
	$(RPCGEN) -l spray.x > $@

rwall.x: $(PROTOCOL_DIR)/rwall.x
	$(RM) rwall.x
	$(CP) $(PROTOCOL_DIR)/rwall.x rwall.x

rwall.h: $(PROTOCOL_DIR)/rwall.h
	$(RM) rwall.h
	$(CP) $(PROTOCOL_DIR)/rwall.h rwall.h

rwall_svc.c: rwall.x
	$(RPCGEN) -A -s datagram_v rwall.x > $@

rwall_clnt.c: rwall.x
	$(RPCGEN) -l -M rwall.x > $@

clean: $(SUBDIRS)
	-$(RM) $(OBJS) $(DERIVED_FILES)

clobber: clean $(SUBDIRS)
	-$(RM) $(PROG) $(CLOBBERFILES)

lint:
	$(LINT.c) $(WCLNTOBJS:%.o=%.c) -lnsl -lrpcsvc -lthread
	$(LINT.c) $(WSVCOBJS:%.o=%.c) -lnsl -lrpcsvc -lthread

FRC:
