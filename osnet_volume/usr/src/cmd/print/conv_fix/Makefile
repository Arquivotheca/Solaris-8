#
# Copyright (c) 1994,1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.9	99/07/12 SMI"
#
# cmd/lp/client/conv_fix/Makefile
#

include 	../Makefile.sp

PROG=		conv_fix

SRCS= 		$(PROG).c

OBJS=		$(SRCS:.c=.o)

ROOTLIBPRINTPROG=	$(PROG:%=$(ROOTLIBPRINT)/%)

FILEMODE=	0555
OWNER=		root

CPPFLAGS +=	-I$(NPRTINC)
LDLIBS +=	$(LIBNPRT)


.KEEP_STATE:

all:		$(PROG)

install:	all $(ROOTLIBPRINTPROG) 

$(ROOTLIBPRINT)/%: %
	$(INS.file)

strip:
	$(STRIP) $(PROG)

lint:
	$(LINT.c) $(PROG).c $(LDLIBS)

cstyle:
	cstyle $(SRCS)

_msg:
	@echo "Messages are made in usr/src/cmd/print"

clean:
	$(RM) $(OBJS)

clobber: clean
	-$(RM) $(PROG) $(CLOBBERFILES)
