# Copyright (c) 1991 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.11	99/10/17 SMI"
#
# cmd/fdformat/Makefile
#

PROG= fdformat

include ../Makefile.cmd

LDLIBS +=	-lvolmgt

.KEEP_STATE:

#
# set-uid root program.
#
$(ROOTPROG) := FILEMODE = 04555
$(ROOTPROG) := OWNER = root

all: $(PROG) 

fdformat: fdformat.c
	$(LINK.c) -o $@ ./fdformat.c $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
