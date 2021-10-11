#
#	Copyright (c) 1988-1992 Sun Microsystems Inc
#	All Rights Reserved.
#
#	"@(#)Makefile	1.11	96/05/23 SMI"
#
# Sample makefile to build the cache manager
#
SED=	sed

PROTOCOL_DIR=$(ROOT)/usr/include/rpcsvc

HDRS=

SRCS=cachemgr.c

OBJS=cachemgr.o nis_cache_svc.o nis_cache_xdr.o

PROG=nis_cachemgr

include $(SRC)/cmd/Makefile.cmd

LDLIBS += -lnsl -lthread

.KEEP_STATE:

all: $(PROG)

install: all $(ROOTUSRSBINPROG)

clean:
	$(RM) $(OBJS)

lint: lint_SRCS

nis_cache.h: $(PROTOCOL_DIR)/nis_cache.x
	$(RPCGEN) -h $(PROTOCOL_DIR)/nis_cache.x |\
	$(SED) -e 's!\"$(PROTOCOL_DIR)/nis_cache.h\"!"nis_cache.h"!' > $@

nis_cache_svc.c: $(PROTOCOL_DIR)/nis_cache.x nis_clnt.h
	$(RPCGEN) -m $(PROTOCOL_DIR)/nis_cache.x |\
	$(SED) -e 's!\"$(PROTOCOL_DIR)/nis_cache.h\"!"nis_cache.h"!' > $@

nis_cache_xdr.c: $(PROTOCOL_DIR)/nis_cache.x nis_clnt.h
	$(RPCGEN) -c $(PROTOCOL_DIR)/nis_cache.x |\
	$(SED) -e 's!\"$(PROTOCOL_DIR)/nis_cache.h\"!"nis_cache.h"!' > $@

nis_clnt.h: $(PROTOCOL_DIR)/nis.x
	$(RPCGEN) -C -h $(PROTOCOL_DIR)/nis.x |\
	$(SED) -n -e '/EDIT_START/,$$ p' |\
	$(SED) -e 's/_3_svc/_svc/' |\
	$(SED) -e 's/_3/_clnt/' > $@

nis_cachemgr: $(OBJS) nis_clnt.h
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

%.o: %.c nis_cache.h nis_clnt.h
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)


include $(SRC)/cmd/Makefile.targ
