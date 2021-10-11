#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.2	99/08/16 SMI"

include ../Makefile.cmd

PROG=	kstat
TARGET= all

.KEEP_STATE:

all: $(PROG)

install: all .WAIT $(ROOTPROG)

_msg: $(PROG).po

clean:

%: %.pl
	$(SED) -e "s@TEXT_DOMAIN@\"$(TEXT_DOMAIN)\"@" $< > $@

$(ROOTBINPROG): $(PROG)
	$(INS.file)

$(PROG).po: $(PROG).pl
	xgettext -d $(PROG) $(PROG).pl 

include ../Makefile.targ
