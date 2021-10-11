#
#ident	"@(#)Makefile	1.3	92/07/14 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

FSTYPE=		nfs
TYPEPROG=	rquotad
ATTMK=		$(TYPEPROG)

include ../../Makefile.fstype

OBJS=	rpc.rquotad.o rquota_xdr.o
SRCS=	$(OBJS:%.o=%.c)
XFILE=	$(ROOT)/usr/include/rpcsvc/rquota.x

LDLIBS += -lnsl

$(TYPEPROG):	$(OBJS)
		$(LINK.c) -o $@ $(LDLIBS) $(OBJS)
		$(POST_PROCESS)

lint:		lint_SRCS

clean:
		$(RM) $(OBJS) rquota_xdr.c

rquota_xdr.c:	$(XFILE)
		$(RPCGEN) -c $(XFILE) -o $@
