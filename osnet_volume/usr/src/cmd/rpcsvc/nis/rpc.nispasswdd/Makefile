#
# Copyright (c) 1994-1997,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.10	99/12/06 SMI"
#
#	Makefile for nispasswd daemon
#
# cmd/rpcsvc/nis/rpc.nispasswdd/Makefile
#

PROG = rpc.nispasswdd

DERIVED_FILES = nispasswd_xdr.c

SRCS = rpc.nispasswdd.c npd_svc.c npd_cache.c npd_svcsubr.c npd_ypfwd.c $(DERIVED_FILES)

OBJS = $(SRCS:%.c=%.o)

include $(SRC)/cmd/Makefile.cmd

PROTOCOL_DIR = $(ROOT)/usr/include/rpcsvc
LDLIBS += -lnsl # temp not link this until switch.so is fixed -lthread

# rpcgen flags
RPCGENFLAGS= -c -C -M


CPPFLAGS += -I$(SRC)/lib/libnsl/include -DDEBUG -D_REENTRANT

#
LINTFLAGS= -L$(SRC)/lib/libnsl -c
LINTOUT= lint.out

.KEEP_STATE:

all := TARGET= all
install := TARGET= install
clean := TARGET= clean
lint := TARGET= lint

all: $(DERIVED_FILES) $(PROG)
install: all $(ROOTUSRSBINPROG)
clean:
	$(RM) core $(DERIVED_FILES) $(OBJS) $(PROG)
lint:
	$(LINT.c) $(SRCS) $(LDLIBS) > $(LINTOUT) 2>&1

#
# build rules
#
$(PROG): $(OBJS)
	$(LINK.c) -o $(PROG) $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

$(DERIVED_FILES) : $(PROTOCOL_DIR)/nispasswd.x
	$(RPCGEN) $(RPCGENFLAGS) $(PROTOCOL_DIR)/nispasswd.x > $(DERIVED_FILES)

include $(SRC)/cmd/Makefile.targ

