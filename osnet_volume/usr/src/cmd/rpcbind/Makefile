#
#ident	"@(#)Makefile	1.13	99/10/17 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/rpcbind/Makefile

PROG= rpcbind


OBJS= check_bound.o pmap_svc.o rpcb_svc.o rpcb_svc_com.o rpcb_svc_4.o \
	 rpcb_stat.o rpcbind.o warmstart.o selfcheck.o


SRCS= $(OBJS:%.o=%.c) ../fs.d/nfs/lib/selfcheck.c

include ../Makefile.cmd

CPPFLAGS= -I. -DPORTMAP $(CPPFLAGS.master)
LDLIBS += -lsocket -lnsl -ldl

.KEEP_STATE:

all: $(PROG) 

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

selfcheck.o:    ../fs.d/nfs/lib/selfcheck.c
		$(COMPILE.c) ../fs.d/nfs/lib/selfcheck.c
		$(POST_PROCESS_O)

install: all $(DIRS) $(ROOTUSRSBINPROG)

$(DIRS):
	$(INS.dir)

clean:
	$(RM) $(OBJS)

lint: lint_SRCS

include ../Makefile.targ
