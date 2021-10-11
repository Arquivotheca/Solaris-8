#
#ident  "@(#)Makefile 1.24     97/06/09 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/fs.d/ufs/fsck/Makefile

FSTYPE=		ufs
LIBPROG=	fsck
ATTMK=		$(LIBPROG)

include		../../Makefile.fstype
include		../Makefile.roll

FSCKOBJS=	main.o dir.o inode.o pass1.o pass1b.o pass2.o \
		pass3.o pass3b.o pass4.o pass5.o setup.o \
		utilities.o
FSCKSRCS=	$(FSCKOBJS:%.o=%.c)

UFSDIR=		../../../../uts/common/fs/ufs
UFSOBJS=	ufs_subr.o ufs_tables.o
UFSSRCS=	$(UFSOBJS:%.o=%.c)
ROLLDIR=	../roll_log

OBJS=		$(FSCKOBJS) $(UFSOBJS) $(ROLLOBJS)
SRCS=		$(FSCKSRCS) $(UFSSRCS) $(ROLLSRCS)

CPPFLAGS +=	-D_LARGEFILE64_SOURCE
LDLIBS += -lc2
$(LIBPROG):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

%.o:	$(UFSDIR)/%.c
	$(COMPILE.c) $<

lint:	lint_SRCS

clean:     
	$(RM) $(FSCKOBJS) $(UFSOBJS)
