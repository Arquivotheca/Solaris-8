#
#ident	"@(#)Makefile	1.10	99/03/21 SMI"
#
# Copyright (c) 1989-1996 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG=	getent

include ../Makefile.cmd

OBJECTS=	getent.o dogetpw.o dogetgr.o dogethost.o dogetipnodes.o \
	dogetserv.o dogetproto.o dogetethers.o dogetnet.o dogetnetmask.o
SRCS=	$(OBJECTS:.o=.c)

LDLIBS	+= -lsocket -lnsl

#
# for message catalog
#
POFILE= gettent.po
POFILES= $(SRCS:%.c=%.po)

.KEEP_STATE:

all: $(PROG)

$(POFILE):      $(POFILES)
	$(RM)	$@
	cat     $(POFILES)      > $@


$(PROG): $(OBJECTS)
	$(LINK.c) -o $@ $(OBJECTS) $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG)

clean:
	$(RM) $(OBJECTS)

lint:	lint_SRCS

include ../Makefile.targ
