#
#pragma ident	"@(#)Makefile	1.6	99/02/13 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#

FSTYPE=		udfs
LIBPROG=		mount
SRCS=		mount.c ../../fslib.c
OBJS=		mount.o fslib.o

include		../../Makefile.fstype

CPPFLAGS += -I../.. -D_LARGEFILE64_SOURCE
LDLIBS += -ladm

# for messaging catalog
#
POFILE= mount.po

# for messaging catalog
#
catalog:        $(POFILE)

$(POFILE):      $(SRCS)
	$(RM) $@
	$(COMPILE.cpp) $(SRCS)   > $(POFILE).i
	$(XGETTEXT) $(XGETFLAGS)        $(POFILE).i
	sed "/^domain/d" messages.po >  $@
	$(RM) $(POFILE).i messages.po

$(LIBPROG):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

fslib.o: ../../fslib.c
	$(COMPILE.c) -o $@ ../../fslib.c

lint:
	$(LINT.c) -mu $(SRCS)

clean:     
	-$(RM) $(OBJS)

putback:
	-cstyle -p mount.c
	-keywords mount.c
	-keywords Makefile
