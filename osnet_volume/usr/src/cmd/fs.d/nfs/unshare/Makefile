#
#ident	"@(#)Makefile	1.14	99/02/21 SMI"
#
# Copyright (c) 1989, 1995, 1999 by Sun Microsystems, Inc.
# All rights reserved.
#

FSTYPE=		nfs
LIBPROG=	unshare
ATTMK=		$(LIBPROG)

include		../../Makefile.fstype

COMMON=		sharetab.o nfslogtab.o
OBJS= 		$(LIBPROG).o $(COMMON) 
SRCS=		$(LIBPROG).c ../lib/sharetab.c ../lib/nfslogtab.c
LDLIBS +=	-lnsl
CFLAGS += -o $@

$(LIBPROG):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

sharetab.o:	../lib/sharetab.c
		$(COMPILE.c) ../lib/sharetab.c

nfslogtab.o:	../lib/nfslogtab.c
		$(COMPILE.c) ../lib/nfslogtab.c

lint:	lint_SRCS

clean:     
	$(RM) $(OBJS)
