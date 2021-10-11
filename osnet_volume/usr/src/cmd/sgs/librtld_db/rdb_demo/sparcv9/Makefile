#
#ident	"@(#)Makefile	1.4	98/11/10 SMI"
#
# Copyright (c) 1996-1998 by Sun Microsystems, Inc.
# All rights reserved.
#

include		../Makefile.com

CPPFLAGS +=	-D_SYSCALL32

# DEMO DELETE START
CCVERBOSE=

LDFLAGS=	-L ../../$(MACH64) \
		-L ../../../libelf/$(MACH64)

LINTFLAGS64 +=	-errchk=longptr64 -D__sparcv9
# DEMO DELETE END

DEMOCFLAGS=	-xarch=v9

.KEEP_STATE:

all:		$(OBJDIR) .WAIT $(PROG)

test: $(TESTS)


# DEMO DELETE START
DEMOCFLAGS=

install:	all $(ROOTONLDBINPROG64)

lint:		$(LINTOUT)
# DEMO DELETE END

include		../Makefile.targ

# DEMO DELETE START
include		../../../../../Makefile.master.64
# DEMO DELETE END
