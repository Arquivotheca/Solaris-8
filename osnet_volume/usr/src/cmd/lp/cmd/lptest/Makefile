#
# ident	"@(#)Makefile	1.10	99/08/16 SMI"
#
# Copyright (c) 1990, 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/lp/cmd/lptest/Makefile
#

include 	../../Makefile.lp

PROG=		lptest

SRCS= 		lptest.c

OBJS=		$(SRCS:.c=.o)

CPPFLAGS =	-I$(LPINC) $(CPPFLAGS.master)
LDLIBS +=	$(LIBBSD) $(LIBREQ) $(LIBMSG) $(LIBOAM)

.KEEP_STATE:

all:		$(PROG)

install:	all .WAIT $(ROOTUSRUCBPROG)

_msg:
		@echo "Messages are made in usr/src/cmd/lp"

clean:
		$(RM) $(OBJS)

clobber:	clean
		-$(RM) $(PROG) $(CLOBBERFILES)

strip:
		$(STRIP) $(PROG)

lint:
		$(LINT.c) $(PROG).c $(LDLIBS)
