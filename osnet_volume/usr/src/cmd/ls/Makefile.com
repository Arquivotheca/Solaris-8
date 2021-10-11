#
#ident	"@(#)Makefile.com	1.1	97/10/01 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/ls/Makefile.com
#

PROG=		ls
XPG4PROG=	ls
OBJS=           $(PROG).o
SRCS=           $(OBJS:%.o=../%.c)

include ../../Makefile.cmd

CFLAGS	+=	-v
$(XPG4) := CFLAGS += -DXPG4
CFLAGS64 +=	-v
CPPFLAGS += -D_FILE_OFFSET_BITS=64
LINTFLAGS =	-x
LINTFLAGS64 +=	-errchk=longptr64

.KEEP_STATE:

all:	$(PROG) $(XPG4)

lint:	lint_SRCS

clean:
	$(RM) $(CLEANFILES)

include ../../Makefile.targ

%.xpg4: ../%.c
	$(LINK.c) -o $@ $< $(LDLIBS)
	$(POST_PROCESS)

%: ../%.c
	$(LINK.c) -o $@ $< $(LDLIBS)
	$(POST_PROCESS)
