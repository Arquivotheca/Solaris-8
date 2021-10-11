#
#ident	"@(#)Makefile	1.2	95/04/06 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
#

PROG= alias
LINKS= bg cd command fc fg getopts hash jobs kill read test type ulimit umask unalias wait
ROOTLINKS= $(LINKS:%=$(ROOTBIN)/%)

SRCS= $(PROG:%=%.sh)

include ../../Makefile.cmd

.KEEP_STATE:

all: $(PROG)

install: all $(ROOTPROG) $(ROOTLINKS)

$(ROOTLINKS): $(ROOTPROG)
		$(RM) $@; $(LN) $(ROOTPROG) $@

clean:

lint:

include ../../Makefile.targ
