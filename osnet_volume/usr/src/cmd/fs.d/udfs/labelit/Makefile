#
#pragma ident	"@(#)Makefile	1.7	99/10/17 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#

FSTYPE=		udfs
LIBPROG=	labelit
SRCS=		labelit.c ud_lib.c
ATTMK=		$(LIBPROG)

include		../../Makefile.fstype

LDLIBS += -lm -ladm
CPPFLAGS +=	-D_LARGEFILE64_SOURCE

labelit : labelit.o  ud_lib.o
	$(LINK.c) -o $@ labelit.o ud_lib.o $(LDLIBS)
	$(POST_PROCESS)

labelit.o : labelit.c
	rm -f ud_lib.h
	cp ../fstyp/ud_lib.h ud_lib.h
	$(COMPILE.c) -o $@ labelit.c
	$(POST_PROCESS_O)

ud_lib.o : ../fstyp/ud_lib.c
	rm -f ud_lib.c ud_lib.h
	cp ../fstyp/ud_lib.h ud_lib.h
	cp ../fstyp/ud_lib.c ud_lib.c
	$(COMPILE.c) -o $@ ud_lib.c
	$(POST_PROCESS_O)

# for messaging catalog
#
POFILE= labelit.po

# for messaging catalog
#
catalog:	$(POFILE)

$(POFILE):	$(SRCS)
	$(RM) $@
	$(COMPILE.cpp) $(SRCS) > $(POFILE).i
	$(XGETTEXT) $(XGETFLAGS) $(POFILE).i
	sed "/^domain/d" messages.po > $@
	$(RM) $(POFILE).i messages.po

clean :
	rm -f labelit.o ud_lib.o
	rm -f ud_lib.c ud_lib.h

lint:
	rm -f ud_lib.h
	cp ../fstyp/ud_lib.h ud_lib.h
	cp ../fstyp/ud_lib.c ud_lib.c
	$(LINT.c) -mu $(SRCS)

putback:
	-cstyle -p labelit.c
	-keywords labelit.c
	-keywords Makefile
