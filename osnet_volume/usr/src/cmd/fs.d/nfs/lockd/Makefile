#
# ident	"@(#)Makefile	1.19	96/07/14 SMI"
#
# Copyright (c) 1990,1994,1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/fs.d/nfs/lockd/Makefile

FSTYPE=		nfs
TYPEPROG=	lockd
ATTMK=		$(TYPEPROG)

include		../../Makefile.fstype

LOCAL=		lockd.o
OBJS=		$(LOCAL) nfs_tbind.o
SRCS=		$(LOCAL:%.o=%.c) ../lib/nfs_tbind.c
LDLIBS +=	-lnsl
CPPFLAGS +=	-I../lib

#
# Message catalog
#
POFILE= lockd.po

#
# message catalog
#
catalog:        $(POFILE)

$(POFILE):      $(SRCS)
	$(RM) $@
	$(COMPILE.cpp) $(SRCS)   > $(POFILE).i
	$(XGETTEXT) $(XGETFLAGS)        $(POFILE).i
	sed "/^domain/d"        messages.po     > $@
	$(RM) messages.po $(POFILE).i


$(TYPEPROG):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

nfs_tbind.o:	../lib/nfs_tbind.c
		$(COMPILE.c) ../lib/nfs_tbind.c

lint:		$(SRCS) lint_SRCS

clean:
		$(RM) $(OBJS)

# Build the .o's with the following CFLAGS for best results with
# Purify.  Remove the .o's before trying to build a regular
# (non-Purify) lockd.
# CFLAGS = -g $($(MACH)_CFLAGS) -DPURIFY

lockd.pure:	$(OBJS)
		purify $(LINK.c) -o $@ $(OBJS) $(LDLIBS)
