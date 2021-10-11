#
# ident	"@(#)Makefile	1.14	99/08/16 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/lp/cmd/lpstat/Makefile
#

include 	../../Makefile.lp

PROG=		lpstat

SRCS =		accept.c	\
		add_mounted.c	\
		charset.c	\
		class.c		\
		device.c	\
		done.c		\
		form.c		\
		lpstat.c	\
		output.c	\
		parse.c		\
		printer.c	\
		request.c	\
		send_message.c

OBJS=		$(SRCS:.c=.o)


CPPFLAGS =	-I$(LPINC) $(CPPFLAGS.master)

LPLIBS =	$(LIBACC)	\
		$(LIBCLS)	\
		$(LIBMSG)	\
		$(LIBPRT)	\
		$(LIBFRM)	\
		$(LIBOAM)	\
		$(LIBLP)

SYSLIBS=	-lcurses

LDLIBS +=	$(LPLIBS) $(SYSLIBS) $(I18N)

.KEEP_STATE:

all:		$(PROG)

$(PROG):	$(OBJS)
		$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

install:	all $(ROOTLIBLPLOCLPROG)

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
