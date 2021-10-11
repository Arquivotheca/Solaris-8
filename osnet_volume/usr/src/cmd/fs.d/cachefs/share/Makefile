#
#ident	"@(#)Makefile	1.2	99/05/25 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/fs.d/cachefs/share
#

FSTYPE=		cachefs
LIBPROG=	share
ATTMK=		$(LIBPROG)

include		../../Makefile.fstype

PROGOBJS=	$(PROGSRCS:%.c=%.o)

PROGSRCS=	$(LIBPROG:%=%.c)

$(LIBPROG): 	$(PROGOBJS)
	$(LINK.c) -o $@ $(PROGOBJS)
	$(POST_PROCESS)

clean:
	$(RM) $(PROGOBJS) $(LIBPROG)
