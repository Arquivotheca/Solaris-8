#
#ident	"@(#)Makefile	1.10	96/04/18 SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/fs.d/ufs/quotacheck/Makefile

FSTYPE=		ufs
LIBPROG=	quotacheck
ATTMK=		$(LIBPROG)

OTHERINSTALL=	$(ROOTUSRSBIN)/$(LIBPROG)
LINKVALUE=	../lib/fs/$(FSTYPE)/$(LIBPROG)

include		../../Makefile.fstype
CPPFLAGS +=	-D_LARGEFILE64_SOURCE
LDLIBS	+=	-ladm -ldl
OBJS=		quotacheck.o
FSOBJS=		preenlib.o
FSSRCS=		../../preenlib.c
SRCS=		$(OBJS:.o=.c)

lint:		lint_PROG

$(FSOBJS): $(FSSRCS)
	$(COMPILE.c) -o $@ $(FSSRCS)

$(LIBPROG):	$(OBJS) $(FSOBJS)
		$(LINK.c) -o $@ $(OBJS) $(FSOBJS) $(LDLIBS)
		$(POST_PROCESS)

clean:
	-$(RM) $(OBJS) $(FSOBJS)

$(ROOTUSRSBIN)/$(LIBPROG):
		-$(RM) $@; $(SYMLINK) $(LINKVALUE) $@
