#
#ident	"@(#)Makefile	1.8	92/12/15 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

# build two versions, dynamic and static
PROG= sync 
STATPROG= sync 

# install rule for static version
# deliberately ahead of a similar rule in ../Makefile.cmd
#
ROOT= /proto
$(ROOT)/sbin/%: %.static
	$(INS.rename)

include ../Makefile.cmd

CLOBBERFILES= $(STATIC)

.KEEP_STATE:

all: $(PROG) $(STATIC) 

install: all  $(ROOTSBINPROG) $(ROOTUSRSBINPROG)
	-$(RM) $(ROOTPROG)
	-$(SYMLINK) ../../sbin/$(PROG) $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
