#
#ident	"@(#)Makefile	1.7	99/08/24 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/rpcsvc/rpc.bootparamd/Makefile

PROG= rpc.bootparamd

OBJS=	bootparam_ip_route.o bootparam_prot_svc.o \
	bootparam_prot_xdr.o bootparam_subr.o

include ../../Makefile.cmd

CPPFLAGS= -I. $(CPPFLAGS.master)
CFLAGS += -DSYSV
LDLIBS += -lsocket -lnsl

SRCS= $(OBJS:%.o=%.c)

.KEEP_STATE:

all: $(PROG)

# multi-object targets

rpc.bootparamd:	$(OBJS)
		$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

install: all $(ROOTUSRSBINPROG)

clean:
	$(RM) $(OBJS)

lint: lint_SRCS

include ../../Makefile.targ
