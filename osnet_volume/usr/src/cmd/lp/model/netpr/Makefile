#
# ident	"@(#)Makefile	1.8	99/08/16 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
#
# cmd/lp/model/netpr
#

PROG=		netpr

include		../../Makefile.lp

PURIFYOPTS      = -logfile=/tmp/errs.%p
PURIFY          = purify $(PURIFYOPTS)

LIBNPRT =	-L$(ROOT)/usr/lib -lprint $(LIBINTL)


CPPFLAGS =	-I. -I$(LPINC) 	\
		$(CPPFLAGS.master)

HDRS=				\
		netpr.h		\
		netdebug.h

SRCS=				\
		netpr.c		\
		misc.c		\
		net.c		\
		tcp_misc.c	\
		bsd_misc.c

OBJS=		$(SRCS:.c=.o)


LPLIBS =			\
		$(LIBMSG)	\
		$(LIBFRM)	\
		$(LIBREQ)	\
		$(LIBPRT)	\
		$(LIBCLS)	\
		$(LIBACC)	\
		$(LIBFLT)	\
		$(LIBUSR)	\
		$(LIBOAM)	\
		$(LIBLP)	\
		$(LIBSEC)	\
		$(LIBSYS)	\
		$(LIBNPRT)

# SYSLIBS=	-lcurses -lgen -lcurses -lnsl
SYSLIBS=	-lgen -lnsl -lsocket

LDLIBS +=	$(LPLIBS) $(SYSLIBS) -lxfn 
ROOTLIBLPBIN =		$(ROOTLIBLP)/bin

ROOTNETPRPROG =            $(PROG:%=$(ROOTLIBLPBIN)/%)

FILEMODE=	04511
OWNER=		root
GROUP=		bin


.KEEP_STATE:

all:		$(PROG)

install:	all $(ROOTLIBLPBIN) $(ROOTNETPRPROG)

_msg:
		@echo "Messages are made in usr/src/cmd/lp"

$(ROOTLIBLPBIN):
	$(INS.dir)

$(ROOTLIBLPBIN)/%:	%
	$(INS.file)


$(PROG):	$(OBJS)
		$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

$(PROG).pure:	$(OBJS)
		$(PURIFY) $(LINK.c) $(OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

clean:
		$(RM) $(OBJS)

clobber:	clean
		-$(RM) $(PROG) $(CLOBBERFILES)

strip:
		$(STRIP) $(PROG)

cstyle:
		cstyle $(SRCS)

LINTFLAGS	+= -L ../../../../lib/print -lprint -lnsl -lsocket
lint:
		$(LINT.c) $(SRCS) $(LDLIBS)
