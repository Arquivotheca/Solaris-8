#
#ident	"@(#)Makefile	1.16	96/04/30 SMI"
#
# Copyright (c) 1995,1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/fs.d/ufs/mount/Makefile

FSTYPE=		ufs
PROG=		mount

include		../../Makefile.fstype

CPPFLAGS += -I../.. -D_LARGEFILE64_SOURCE
LDLIBS += -ladm

OBJS=		$(PROG).o realpath.o fslib.o
SRCS=		$(PROG).c realpath.c ../../fslib.c

$(PROG) $(STATIC):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)


lint:	lint_SRCS

clean:     
	-$(RM) $(OBJS)

# for messaging catalog
#
POFILE= mount.po

catalog:        $(POFILE)

$(POFILE):      $(SRCS)
	$(RM) $@
	$(COMPILE.cpp) $(SRCS)   > $(POFILE).i
	$(XGETTEXT) $(XGETFLAGS)        $(POFILE).i
	sed "/^domain/d" messages.po >  $@
	$(RM) $(POFILE).i messages.po

fslib.o: ../../fslib.c
	$(COMPILE.c) -o $@ ../../fslib.c
