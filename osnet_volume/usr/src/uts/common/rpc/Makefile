#
#ident	"@(#)Makefile	1.32	99/12/06 SMI"
#
# Copyright (c) 1989,1995,1996,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/common/rpc/Makefile
#
# include global definitions
include ../../../Makefile.master

HDRS= \
auth.h		auth_des.h	auth_sys.h	auth_unix.h \
bootparam.h	clnt.h		clnt_soc.h	clnt_stat.h	des_crypt.h \
nettype.h	pmap_clnt.h	pmap_rmt.h \
rac.h		raw.h		rpc.h		rpc_com.h	rpc_msg.h \
rpcb_clnt.h	rpcent.h	svc.h		svc_auth.h	svc_soc.h \
trace.h		types.h		xdr.h		rpcsec_gss.h	svc_mt.h \
rpcsys.h

RPC_SRC=	pmap_prot.x	rpcb_prot.x

RPCSVC_SRC=	key_prot.x	rpc_sztypes.x

DERIVED_FILES=	key_prot.h	pmap_prot.h	rpcb_prot.h	rpc_sztypes.h

RPCHDRS=	$(HDRS)	$(RPC_SRC) $(DERIVED_FILES)

RPCSVCHDRS=	$(RPCSVC_SRC)

RPCDIRS=	$(ROOT)/usr/include/rpc
RPCSVCDIRS=	$(ROOT)/usr/include/rpcsvc

ROOTHDRS= $(RPCHDRS:%=$(RPCDIRS)/%) $(RPCSVCHDRS:%=$(RPCSVCDIRS)/%)

$(RPCDIRS)/%: %
	$(INS.file)

$(RPCSVCDIRS)/%: %
	$(INS.file)

# XXX: should really check the style of the derived files as well...
#	$(RPC_SRC:%.x=%.check) \
#	$(RPCSVC_SRC:%.x=%.check)
#
CHECKHDRS= $(HDRS:%.h=%.check)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS)

all: all_h

install_h: all_h $(RPCDIRS) $(RPCSVCDIRS) $(ROOTHDRS)

# all_h permits derived headers to be built here in the uts source area
# for the kernel to reference, without going so far as to install them.
#
all_h: $(DERIVED_FILES)

clean:
	$(RM) $(DERIVED_FILES)

$(RPCDIRS):
	$(INS.dir)

$(RPCSVCDIRS):
	$(INS.dir)

key_prot.h: key_prot.x
	$(RPCGEN) -C -h key_prot.x > $@

pmap_prot.h: pmap_prot.x
	$(RPCGEN) -h pmap_prot.x > $@


# The sed stuff is a workaround for an rpcgen bug (bugid 1128007).
rpc_sztypes.h: rpc_sztypes.x
	$(RPCGEN) -C -h rpc_sztypes.x > rpc_sztypes.tmp
	sed -e s/ulonglong/u_longlong/ rpc_sztypes.tmp > $@
	rm rpc_sztypes.tmp

rpcb_prot.h: rpcb_prot.x
	$(RPCGEN) -h rpcb_prot.x > $@

check:	$(CHECKHDRS)
