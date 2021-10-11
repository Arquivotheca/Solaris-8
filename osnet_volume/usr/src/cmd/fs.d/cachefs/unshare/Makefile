#
#ident	"@(#)Makefile	1.1	99/05/24 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/fs.d/cachefs/unshare
#

FSTYPE=		cachefs
LIBPROG=	unshare
ATTMK=		$(LIBPROG)

include		../../Makefile.fstype

PROGOBJS=	$(PROGSRCS:%.c=%.o)

PROGSRCS=	$(LIBPROG:%=%.c)

$(LIBPROG): 	$(PROGOBJS)
	$(LINK.c) -o $@ $(PROGOBJS)
	$(POST_PROCESS)

clean:
	$(RM) $(PROGOBJS) $(LIBPROG)
