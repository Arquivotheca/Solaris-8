#
# ident	"@(#)Makefile	1.14	99/08/16 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/lp/cmd/bsd/lpc/Makefile
#

include 	../../Makefile.lp

PROG=		lpc

SRCS= 		cmds.c cmdtab.c lpc.c process.c topq.c fatalmsg.c sndrcv.c \
		global.c

OBJS=		$(SRCS:.c=.o)


CPPFLAGS =	-I. -I$(LPINC) $(CPPFLAGS.master)
LDLIBS +=	$(LIBREQ) $(LIBMSG) $(LIBOAM) \
		$(LIBPRT) $(LIBSEC) $(LIBLP) -lgen -lcurses -lintl -lw

.KEEP_STATE:

.PARALLEL: $(OBJS)

all:		$(PROG)

$(PROG):	$(OBJS)
		$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

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
		$(LINT.c) $(SRCS) $(LDLIBS)
