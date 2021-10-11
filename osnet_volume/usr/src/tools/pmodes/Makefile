#
#ident	"@(#)Makefile	1.1	99/03/22 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# fastfs also lives in cmd/backup, but it's copied here since the backup
# hierarchy can still build some hsm packages, and the code doesn't change

PROG=	pmodes
OBJS=	$(PROG).o
SRCS=	$(OBJS:%.o=%.c)

include ../Makefile.tools

OWNER=	root
GROUP=	bin
CFLAGS += -v

.KEEP_STATE:

all:	$(PROG)

install: all .WAIT $(ROOTONBLDMACHPROG)

lint:	lint_SRCS

clean:
	$(RM) $(CLEANFILES)

include ../Makefile.targ
