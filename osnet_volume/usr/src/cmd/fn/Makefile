#
#ident	"@(#)Makefile	1.34	99/10/17 SMI"
#
# Copyright (c) 1989 - 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# Makefile for the fn utilities
#
# cmd/fn/Makefile
#

PROG = \
    fnbind fnunbind fnlookup fnlist fnrename fnattr fncreate_printer fnsearch
SHFILE = fncheck fnsetup
SBINPROG = fncreate fncreate_fs fndestroy fncheck fnselect fncopy fnsypd

CLOBBERFILES += $(SHFILE) $(SBINPROG)

include ../Makefile.cmd

ROOTPROGS	= $(PROG:%=$(ROOTBIN)/%)

ROOTSBINPROGS	= $(SBINPROG:%=$(ROOTUSRSBIN)/%)

SHSRCS=		$(SHFILE:%=%.sh)

LDLIBS += -lxfn

# CC adds /opt/SUNWspro/lib to the run_path it passes to ld.  This overrides
# it.
RUNPATH= 
fncreate fncopy fncreate_printer fnsypd := RUNPATH = -Qoption ld -R/usr/lib/fn
fncreate fncopy fncreate_printer fnsypd := LDLIBS += -L$(ROOTLIB)/fn
$(PROG) $(SBINPROG) := LDFLAGS += $(RUNPATH)

FNONCSRC=	$(SRC)/lib/fn/context/onc_ns

# The following are for fncreate only
FNCOBJS = fncreate.o fnutils.o fncreate_attr_utils.o
HLIBS = -lfn_p -lnsl $(FNONCSRC)/nisplus/$(MACH)/fn_ctx_onc_fn_nisplus.so.1 \
	$(FNONCSRC)/nis/$(MACH)/fn_ctx_onc_fn_nis.so.1

# The following are for fncopy only
COBJS = fncopy.o fncreate_attr_utils.o fncopy_utils.o
CLIBS = -lfn_spf -lfn_p -lnsl \
	$(FNONCSRC)/nisplus/$(MACH)/fn_ctx_onc_fn_nisplus.so.1 \
	$(FNONCSRC)/nis/$(MACH)/fn_ctx_onc_fn_nis.so.1

# The following are for fncreate_printer only
POBJS = fncreate_printer.o fncreate_attr_utils.o
PLIBS = -lfn_p -lnsl -lfnsp $(FNONCSRC)/nisplus/$(MACH)/fn_ctx_onc_fn_nisplus.so.1 

# The following are for fnsypd only
FNSYPOBJS = fnsypd.o fnsypprot_svc.o fnsypprot_xdr.o
FNSYPLIBS = -lnsl -ldl $(FNONCSRC)/nis/$(MACH)/fn_ctx_onc_fn_nis.so.1
FNSYPSRC = fnsypprot.x fnsypprot.h fnsypprot_svc.c \
	fnsypprot_clnt.c fnsypprot_xdr.c 

# The following are for fncreate_fs only
FSOBJS = fncreate_fs.o fs_parse_utils.o fs_ctx_utils.o Tree.o
FSOBJS2 = fncreate_attr_utils.o 
FSLIBS = -lnsl -lfn_p
FSHDRS = $(SRC)/lib/fn/libxfn/common

FNINCDIR = $(SRC)/lib/fn/include
CPPFLAGS += -I$(FNINCDIR)

$(FSOBJS) :=	CPPFLAGS += -I$(FSHDRS)

debug :=	CPPFLAGS += $(DEBUG)
debug :=	COPTFLAG = -g
debug :=	CCOPTFLAG = -g
tcov :=		CFLAGS += -xa
tcov :=		CCFLAGS += -a

CLEANFILES += $(FNCOBJS) $(COBJS) $(FSOBJS) $(FSOBJS2) $(POBJS) \
	$(FNSYPOBJS) $(FNSYPSRC) \
	$(PROG:%=%.o) fndestroy.o fnselect.o fnattr_utils.o

.KEEP_STATE:

all: $(PROG) $(SBINPROG)

debug install: all $(SHFILE) $(ROOTBIN) $(ROOTUSRSBIN) \
	$(ROOTPROG) $(ROOTSBINPROGS)

install_h:

%.o: %.cc
	$(COMPILE.cc) -o $@ $<
	$(POST_PROCESS_O)

fncreate: $(FNCOBJS)
	$(LINK.cc) -o $@ $(FNCOBJS) $(LDLIBS) $(HLIBS)
	$(POST_PROCESS)

fncreate_printer: $(POBJS)
	$(LINK.cc) -o $@ $(POBJS) $(LDLIBS) $(PLIBS)
	$(POST_PROCESS)

fncreate_fs: $(FSOBJS) $(FSOBJS2)
	$(LINK.cc) -o $@ $(FSOBJS) $(FSOBJS2) $(LDLIBS) $(FSLIBS)
	$(POST_PROCESS)

fnsypprot.x: $(FNONCSRC)/nis/common/fnsypprot.x
	$(CP) $(FNONCSRC)/nis/common/fnsypprot.x .

fnsypprot.h: fnsypprot.x
	$(RPCGEN) -A -C -h -o fnsypprot.h fnsypprot.x

fnsypd.c: fnsypprot.h

fnsypprot_svc.c: fnsypprot.h fnsypprot.x
	$(RPCGEN) -A -C fnsypprot.x

fnsypprot_xdr.c: fnsypprot.h fnsypprot.x
	$(RPCGEN) -A -C -c -o fnsypprot_xdr.c fnsypprot.x

fnsypd: fnsypprot.h $(FNSYPOBJS)
	$(LINK.cc) -o $@ $(FNSYPOBJS) $(LDLIBS) $(FNSYPLIBS)
	$(POST_PROCESS)

fndestroy: fndestroy.o
	$(LINK.cc) -o $@ fndestroy.o $(LDLIBS)
	$(POST_PROCESS)

fnbind: fnbind.o
	$(LINK.cc) -o $@ fnbind.o $(LDLIBS) -lnsl
	$(POST_PROCESS)

fnunbind: fnunbind.o
	$(LINK.cc) -o $@ fnunbind.o $(LDLIBS)
	$(POST_PROCESS)

fnlookup: fnlookup.o
	$(LINK.cc) -o $@ fnlookup.o $(LDLIBS)
	$(POST_PROCESS)

fnlist: fnlist.o
	$(LINK.cc) -o $@ fnlist.o $(LDLIBS)
	$(POST_PROCESS)

fnattr: fnattr.o fnattr_utils.o
	$(LINK.cc) -o $@ fnattr.o fnattr_utils.o $(LDLIBS)
	$(POST_PROCESS)

fnrename: fnrename.o 
	$(LINK.cc) -o $@ fnrename.o $(LDLIBS)
	$(POST_PROCESS)

fnsearch: fnsearch.o fnattr_utils.o
	$(LINK.cc) -o $@ fnsearch.o fnattr_utils.o $(LDLIBS)
	$(POST_PROCESS)

fnselect: fnselect.o 
	$(LINK.cc) -o $@ fnselect.o -lfn_p -lnsl $(LDLIBS)
	$(POST_PROCESS)

fncopy: $(COBJS)
	$(LINK.cc) -o $@ $(COBJS) $(LDLIBS) $(CLIBS)
	$(POST_PROCESS)

$(ROOTBIN) $(ROOTUSRSBIN) :
	$(INS.dir)

clean:
	-$(RM) $(CLEANFILES)

# disable linting since it is useless for C++ code
lint:

include ../Makefile.targ
