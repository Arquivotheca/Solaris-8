#
#ident	"@(#)Makefile	1.2	96/09/10 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#

include		../Makefile.com

.KEEP_STATE:

all:		$(OBJDIR) .WAIT $(PROG)

test: $(TESTS)

# DEMO DELETE START
install:	all $(ROOTONLDBINPROG)

lint:		$(LINTOUT)
# DEMO DELETE END

include		../Makefile.targ
