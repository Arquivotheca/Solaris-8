#
#ident	"@(#)Makefile	1.5	96/06/10 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
#

PROG= clear_locks

include         ../../../Makefile.cmd

LDLIBS		+=	-lrpcsvc -lnsl

#
# Message catalog
#
POFILE= clear_locks.po

#
# message catalog
#
catalog:        $(POFILE)

$(POFILE):      clear_locks.c
	$(RM) $@
	$(COMPILE.cpp) clear_locks.c   > $(POFILE).i
	$(XGETTEXT) $(XGETFLAGS)        $(POFILE).i
	sed "/^domain/d"        messages.po     > $@
	$(RM) messages.po $(POFILE).i


CFLAGS		+=	-v

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTUSRSBINPROG)

clean:

lint:	lint_PROG

include ../../../Makefile.targ
