#
#ident	"@(#)Makefile	1.7	99/02/13 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#

FSTYPE=		udfs
LIBPROG=	mkfs
ATTMK=		$(LIBPROG)

include		../../Makefile.fstype

# for messaging catalog
#
POFILE= mkfs.po

catalog:        $(POFILE)


LDLIBS += -lintl -lm -ladm

MKFSOBJS=	mkfs.o udfslib.o

UDFSDIR=	../../../../uts/common/fs/ufs
#UDFSOBJS=	ufs_subr.o ufs_tables.o
UDFSOBJS=

OBJS=		$(MKFSOBJS) $(UDFSOBJS)
SRCS=		$(OBJS:%.o=%.c)

LDLIBS += -lc2 -ladm

$(LIBPROG):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

%.o:	$(UDFSDIR)/%.c
	$(COMPILE.c) $<

lint:
	$(LINT.c) -mu $(SRCS)

clean:     
	$(RM) $(OBJS)

putback:
	-cstyle -p mkfs.c
	-cstyle -p udfslib.c
	-keywords mkfs.c
	-keywords udfslib.c
	-keywords Makefile
