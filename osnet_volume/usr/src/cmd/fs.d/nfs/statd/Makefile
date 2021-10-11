#
#
# Copyright (c) 1990-1999 by Sun Microsystems, Inc.
#
# cmd/fs.d/nfs/statd/Makefile
#

#pragma ident	"@(#)Makefile	1.16	99/04/27 SMI"

FSTYPE=		nfs
TYPEPROG=	statd
ATTMK=		$(TYPEPROG)

include		../../Makefile.fstype
CPPFLAGS +=     -D_REENTRANT -DSUN_THREADS

OBJS= 		sm_svc.o sm_proc.o sm_statd.o selfcheck.o 

SRCS=		$(OBJS:%.o=%.c) ../lib/selfcheck.c

LDLIBS +=	-lsocket -lrpcsvc -lnsl -lthread

$(TYPEPROG):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)
		$(LOCK_LINT)

selfcheck.o:	../lib/selfcheck.c
		$(COMPILE.c) ../lib/selfcheck.c

lint:		lint_SRCS

clean:
		$(RM) $(OBJS) $(TYPEPROG)



