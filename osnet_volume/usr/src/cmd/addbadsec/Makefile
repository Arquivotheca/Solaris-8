#
#ident "@(#)Makefile	1.3	96/03/18 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= addbadsec

OBJECTS=	addbadsec.o  ix_altsctr.o
SRCS=		$(OBJECTS:.o=.c)

include ../Makefile.cmd

.KEEP_STATE:

all: $(PROG) $(PROG).mk

$(PROG):        $(OBJECTS)
	$(LINK.c) -o $@ $(OBJECTS) $(LDLIBS)

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
