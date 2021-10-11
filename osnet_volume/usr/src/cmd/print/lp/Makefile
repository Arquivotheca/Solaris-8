#
# Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
# All Rights Reserved
#
# ident "@(#)Makefile	1.7	99/07/12 SMI"
#
# cmd/lp/client/lp/Makefile
#

include 	../Makefile.sp

PROG=		lp

SRCS= 		$(PROG).c

OBJS=		$(SRCS:.c=.o)

ROOTBINPROG=	$(PROG:%=$(ROOTBIN)/%)
ROOTUSRBINSYMLINK=	$(ROOTUSRUCB)/lpr
PRINTSYMLINK=	$(ROOTLIB)/print/printd
$(ROOTLIB)/print	:=	OWNER=root
$(ROOTLIB)/print	:=	GROUP=lp

FILEMODE=	04511
OWNER=		root

CPPFLAGS +=	-I$(NPRTINC)
LDLIBS +=	$(LIBNPRT)

.KEEP_STATE:

all:		$(PROG)

install:	all $(ROOTBIN) $(ROOTLIB)/print $(ROOTBINPROG) \
		$(ROOTUSRBINSYMLINK) $(PRINTSYMLINK)

$(ROOTBIN) $(ROOTLIB)/print:
	$(INS.dir)

$(ROOTUSRBINSYMLINK):
	$(RM) $@; $(SYMLINK) ../bin/$(PROG) $@

$(PRINTSYMLINK):
	$(RM) $@; $(SYMLINK) ../../bin/$(PROG) $@

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
