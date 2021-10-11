#
#ident	"@(#)Makefile	1.12	98/04/22 SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/fs.d/ufs/quota/Makefile

FSTYPE=		ufs
LIBPROG=	quota
ATTMK=		$(LIBPROG)

OTHERINSTALL=	$(ROOTUSRSBIN)/$(LIBPROG)
LINKVALUE=	../lib/fs/$(FSTYPE)/$(LIBPROG)

include		../../Makefile.fstype

OBJS=		quota.o rquota_xdr.o replica.o
SRCS=		$(OBJS:%.o=%.c)
XFILE=		$(ROOT)/usr/include/rpcsvc/rquota.x
CPPFLAGS +=	-D_LARGEFILE64_SOURCE
LDLIBS +=		-lnsl

FILEMODE= 04555
OWNER= root
GROUP= bin

$(LIBPROG):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

lint:		lint_PROG

clean:
		$(RM) $(OBJS) rquota_xdr.c

$(ROOTUSRSBIN)/$(LIBPROG):
		-$(RM) $@; $(SYMLINK) $(LINKVALUE) $@

rquota_xdr.c:	$(XFILE)
		$(RPCGEN) -c $(XFILE) -o $@

replica.o:	../../nfs/lib/replica.c
		$(COMPILE.c) ../../nfs/lib/replica.c
