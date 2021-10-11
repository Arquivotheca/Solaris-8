#
# Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
# All Rights Reserved
#
#ident "@(#)Makefile	1.6	99/07/12 SMI"
#
# cmd/lp/client/lpmove/Makefile
#

include 	../Makefile.sp

PROG=		lpmove

SRCS= 		$(PROG).c

OBJS=		$(SRCS:.c=.o)

ROOTUSRSBINPROG=	$(PROG:%=$(ROOTUSRSBIN)/%)
ROOTUSRLIBSYMLINK=	$(PROG:%=$(ROOTLIB)/%)

FILEMODE=	04511
OWNER=		root

CPPFLAGS +=	-I$(NPRTINC)
LDLIBS +=	$(LIBNPRT)

.KEEP_STATE:

all:		$(PROG)

install:	all $(ROOTUSRSBIN) $(ROOTUSRSBINPROG) $(ROOTUSRLIBSYMLINK)


$(ROOTUSRSBIN):
	$(INS.dir)

$(ROOTUSRLIBSYMLINK):
	$(RM) $@; $(SYMLINK) ../sbin/$(PROG) $@

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
