#
#ident	"@(#)Makefile	1.11	96/03/19 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= bc
LIBB= lib.b
CLOBBERFILES= $(LIBB)

include ../Makefile.cmd

ROOTLIBB= $(LIBB:%=$(ROOTLIB)/%)
$(ROOTLIBB) :=	FILEMODE =	$(LIBFILEMODE)

.KEEP_STATE:

all: $(PROG) $(LIBB) 

$(LIBB): $(LIBB).data
	-$(RM) $(LIBB)
	$(CP) $(LIBB).data $(LIBB)

install: all $(ROOTPROG) $(ROOTLIBB)

clean:
	$(RM) $(PROG).c $(PROG).x

lint:

include ../Makefile.targ
