#
# ident	"@(#)Makefile	1.23	99/08/16 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/lp/cmd/lpsched/lpsched/Makefile
#

PROG=		lpsched

include		../../Makefile.lp

PURIFYOPTS      = -logfile=/tmp/errs.%p
PURIFY          = purify $(PURIFYOPTS)


# Doing -DDEBUG allows some nice log files to be generated
# with the -d option.

CPPFLAGS =	-I. -I$(LPINC) $(CPPFLAGS.master)

HDRS=				\
		nodes.h		\
		dispatch.h	\
		validate.h	\
		lpsched.h

SRCS=				\
		alerts.c	\
		cancel.c	\
		daisyforms.c	\
		disena.c	\
		disp1.c		\
		disp2.c		\
		disp3.c		\
		disp4.c		\
		disp5.c		\
		disptab.c	\
		dowait.c	\
		exec.c		\
		faults.c	\
		files.c		\
		flt.c		\
		fncs.c		\
		getkey.c	\
		init.c		\
		log.c		\
		lpfsck.c	\
		lpsched.c	\
		msgs.c		\
		notify.c	\
		pickfilter.c	\
		ports.c		\
		requeue.c	\
		rstatus.c	\
		schedule.c	\
		status.c	\
		terminate.c	\
		validate.c

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
		$(LIBSEC)

SYSLIBS=	-lcurses -lgen -lcurses -lnsl

LDLIBS +=	$(LPLIBS) $(SYSLIBS)

ROOTSYMLNKPROG =	$(ROOTLIBPROG)
SYMLNKDEST =		lp/$(PROG)

LPSCHED_VERSION_STRING  = ($(ECHO) build info: \\\c; date +%D:%H:%M:%S)

.KEEP_STATE:

all:		$(PROG)

$(PROG):	$(OBJS) $(LPLIBS) lpschedvers.o
		$(LINK.c) $(OBJS) lpschedvers.o -o $@ $(LDLIBS)
		$(POST_PROCESS)

lpschedvers.c:	FRC
	@-$(ECHO) "rebuilding lpschedvers.c"
	@-( \
	$(ECHO) "/*" ; \
	$(ECHO) " * This file is derived from cmd/lpsched/lpsched/Makefile" ; \
	$(ECHO) " */\n" ; \
	$(ECHO) "#pragma\tident\t\"@(#)lpschedvers.c\tDERIVED\t96/2/1 SMI\"\n" ; \
	$(ECHO) "char *lpsched_buildinfo = \"`$(LPSCHED_VERSION_STRING)`\\\\n\";" ; \
	$(ECHO) "" ) > ./lpschedvers.c

$(PROG).pure:	$(OBJS) $(LPLIBS)
		$(PURIFY) $(LINK.c) $(OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

install:	all $(ROOTLIBLPPROG) $(ROOTSYMLNKPROG)

$(ROOTSYMLNKPROG) :
		$(RM) $@; $(SYMLINK) $(SYMLNKDEST) $@

_msg:
		@echo "Messages are made in usr/src/cmd/lp"

clean:
		$(RM) $(OBJS)
		$(RM) lpschedvers.c lpschedvers.o

clobber:	clean
		-$(RM) $(PROG) $(CLOBBERFILES)

strip:
		$(STRIP) $(PROG)

lint:
		$(LINT.c) $(SRCS) $(LDLIBS)

FRC:
