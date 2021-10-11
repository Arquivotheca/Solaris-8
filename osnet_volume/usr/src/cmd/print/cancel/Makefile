#
# ident "@(#)Makefile 1.6	99/07/12 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
# All Rights Reserved
#
# cmd/lp/client/cancel/Makefile
#

include 	../Makefile.sp

PROG=		cancel

SRCS= 		cancel.c cancel_list.c

OBJS=		$(SRCS:.c=.o)

ROOTBINPROG=	$(PROG:%=$(ROOTBIN)/%)
ROOTUSRBINSYMLINK=	$(ROOTUSRUCB)/lprm

FILEMODE=	04511
OWNER=		root

CPPFLAGS +=	-I. -I$(NPRTINC) #$(CPPFLAGS.master)
LDLIBS +=	$(LIBNPRT)

.KEEP_STATE:

all:		$(PROG)

$(PROG):	$(OBJS)
		$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

install:	all $(ROOTBIN) $(ROOTBINPROG) $(ROOTUSRBINSYMLINK)

$(ROOTBIN):
	$(INS.dir)

$(ROOTUSRBINSYMLINK):
	$(RM) $@; $(SYMLINK) ../bin/$(PROG) $@

strip:
	$(STRIP) $(PROG)

lint:
	$(LINT.c) $(SRCS) $(LDLIBS)

cstyle:
	cstyle $(SRCS)

_msg:
	@echo "Messages are made in usr/src/cmd/print"

clean:
	$(RM) $(OBJS)

clobber: clean
	-$(RM) $(PROG) $(CLOBBERFILES)

