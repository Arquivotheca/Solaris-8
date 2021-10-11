#
#ident	"@(#)Makefile	1.1	99/01/11 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# tools/install.bin/Makefile
#

PROG=	install.bin
OBJS=	$(PROG).o
SRCS=	$(OBJS:%.o=%.c)

include ../Makefile.tools

OWNER=	root
GROUP=	bin
CFLAGS += -v
LDLIBS += -lgen

.KEEP_STATE:

all:	$(PROG)

install: all .WAIT $(ROOTONBLDMACHPROG)
	$(RM) $(ROOTONBLDBINMACH)/install
	$(LN) $(ROOTONBLDMACHPROG) $(ROOTONBLDBINMACH)/install

lint:	lint_SRCS

clean:
	$(RM) $(CLEANFILES)

include ../Makefile.targ
