#
#ident	"@(#)Makefile	1.12	96/07/14 SMI"	/* SVr4.0 1.	*/
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

FSTYPE=		nfs
TYPEPROG=	nfsd
ATTMK=		$(TYPEPROG)

include		../../Makefile.fstype

LDLIBS +=	-lnsl

LOCAL=		nfsd.o
OBJS=		$(LOCAL) nfs_tbind.o
SRCS=		$(LOCAL:%.o=%.c) ../lib/nfs_tbind.c
CPPFLAGS +=	-I../lib

$(TYPEPROG):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POSTPROCESS)

nfs_tbind.o:	../lib/nfs_tbind.c
		$(COMPILE.c) ../lib/nfs_tbind.c

clean:
		$(RM) $(OBJS)

lint:		$(SRCS) lint_SRCS
