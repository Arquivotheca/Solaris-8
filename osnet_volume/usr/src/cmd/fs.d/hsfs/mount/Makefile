#
#ident	"@(#)Makefile	1.12	96/06/10 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

FSTYPE=		hsfs
PROG=		mount
SRCS=		mount.c
OBJS=		mount.o fslib.o
ATTMK= 		$(PROG)

include		../../Makefile.fstype

CPPFLAGS += -I../.. 

#
# Message catalog
#
POFILE= mount.po

#
# message catalog
#
catalog: $(POFILE)

$(POFILE): $(SRCS)
	$(RM) $@
	$(COMPILE.cpp) $(SRCS)   > $(POFILE).i
	$(XGETTEXT)     $(XGETFLAGS) $(POFILE).i
	sed "/^domain/d"        messages.po     > $@
	$(RM) $(POFILE).i messages.po


$(PROG) $(STATIC):	$(OBJS)
			$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
			$(POST_PROCESS)

fslib.o: ../../fslib.c
	$(COMPILE.c) ../../fslib.c
