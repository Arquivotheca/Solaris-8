#
#pragma ident	"@(#)Makefile	1.12	98/03/18 SMI"	/* SVr4.0 1.12	*/
#
# Copyright (c) 1989, 1995-1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/fs.d/nfs/nfsstat/Makefile

PROG=		nfsstat

include		../../../Makefile.cmd

CFLAGS +=	-v -o $@
COMMON=	nfs_sec.o
OBJS=	nfsstat.o $(COMMON)
SRCS=	nfsstat.c ../lib/nfs_sec.c

LDLIBS +=		-lkstat -lnsl

$(PROG):	$(OBJS)
		$(LINK.c) -o $@ $(LDLIBS) $(OBJS)
		$(POST_PROCESS)

FILEMODE=       0555
GROUP=          bin

.KEEP_STATE:

all:		$(PROG)

nfs_sec.o:	../lib/nfs_sec.c
		$(COMPILE.c) ../lib/nfs_sec.c

install:	all $(ROOTPROG)

lint:
	$(LINT.c) $(SRCS) $(LDLIBS)

clean:     
	$(RM) $(OBJS)

include ../../../Makefile.targ
