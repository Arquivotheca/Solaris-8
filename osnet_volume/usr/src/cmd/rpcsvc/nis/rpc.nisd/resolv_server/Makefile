# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.3	98/03/16 SMI"
#
# Makefile for the nis+ dns fwd daemon
#

SRVOBJS= main.o \
	ypresolv_proc.o \
	rpc_as.o \
	svc_run_as.o \
	ngethostbyname.o \
	nget_answer.o \
	nres_search.o \
	nres_rcv.o \
	nres_send.o

COMMONOBJS= resolv_common.o

OBJS= $(SRVOBJS) $(COMMONOBJS)
SRCS= $(OBJS:.o=.c)

include $(SRC)/cmd/Makefile.cmd

PROG= rpc.nisd_resolv

CFLAGS += -v

# We don't use -lsocket for 4.x build
LDLIBS += -lnsl -lresolv -lsocket 

.KEEP_STATE:

all: $(PROG)

install: all $(ROOTUSRSBINPROG)

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

resolv_common.o: ../resolv_common.c
	$(COMPILE.c) ../resolv_common.c

lint:
	$(LINT.c) $(SRVOBJS:.o=.c) ../resolv_common.c $(LDLIBS)

clobber:  clean
	$(RM) $(PROG)

clean:
	$(RM) $(OBJS)
