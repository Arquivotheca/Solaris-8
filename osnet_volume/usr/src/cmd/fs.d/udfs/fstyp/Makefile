#
#pragma ident	"@(#)Makefile	1.6	99/10/17 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#

FSTYPE=		udfs
LIBPROG=	fstyp
ATTMK=		$(LIBPROG)

include		../../Makefile.fstype

CFLAGS +=	-v
CPPFLAGS +=	-D_LARGEFILE64_SOURCE
LDLIBS += -ladm

SRCS=	fstyp.c ud_lib.c

# for messaging catalog
#
POFILE= fstyp.po

catalog:        $(POFILE)

$(POFILE):      $(SRCS)
	$(RM) $@
	$(COMPILE.cpp) $(SRCS)   > $(POFILE).i
	$(XGETTEXT) $(XGETFLAGS)        $(POFILE).i
	sed "/^domain/d" messages.po >  $@
	$(RM) $(POFILE).i messages.po

fstyp: fstyp.o ud_lib.o
	$(LINK.c) -o $@ fstyp.o ud_lib.o $(LDLIBS)
	$(POST_PROCESS)

clean :
	rm -f fstyp.o ud_lib.o

lint:
	$(LINT.c) -mu $(SRCS)

putback :
	-cstyle -p fstyp.c
	-cstyle -p ud_lib.c
	-keywords fstyp.c
	-keywords ud_lib.c
	-keywords Makefile
