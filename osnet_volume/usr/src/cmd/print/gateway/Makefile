#
# ident "@(#)Makefile 1.14	99/07/12 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
# All Rights Reserved
#
# server/gateway/adaptor/Makefile
#

include 	../Makefile.sp

SUBDIRS=	translators

PROG=		in.lpd

SRCS= 		main.c adaptor.c

OBJS=		$(SRCS:.c=.o)


FILEMODE=	0555
OWNER=		root
GROUP=		bin


CPPFLAGS +=	-I. -I$(NPRTINC) #$(CPPFLAGS.master)
LDLIBS +=	-lsocket -lnsl -ldl $(LIBNPRT)

.KEEP_STATE:

all:		$(PROG) $(SUBDIRS)

$(PROG):	$(OBJS)
		$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

install:	all $(ROOTLIBPRINTPROG) $(SUBDIRS)

strip:		$(SUBDIRS)
		$(STRIP) $(PROG)

cstyle:
		cstyle $(SRCS)

lint:
		$(LINT.c) $(SRCS) $(LDLIBS)

clean clobber:	$(SUBDIRS)
		$(RM) $(OBJS)

_msg:
		@echo "Messages are made in usr/src/cmd/print"

$(SUBDIRS):	FRC
		@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
