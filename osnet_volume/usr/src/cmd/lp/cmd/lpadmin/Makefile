#
# ident	"@(#)Makefile	1.16	99/08/16 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/lp/cmd/lpadmin/Makefile
#

include		../../Makefile.lp

CPPFLAGS =	-D_REENTRANT -I. -I$(LPINC) $(CPPFLAGS.master)

SRCS=		chkopts.c	\
		default.c	\
		do_align.c	\
		do_fault.c	\
		do_mount.c	\
		do_printer.c	\
		do_pwheel.c	\
		done.c		\
		fromclass.c	\
		ismodel.c	\
		lpadmin.c	\
		options.c	\
		output.c	\
		pick_opts.c	\
		rmdest.c	\
		send_message.c	\
		signals.c	\
		startup.c	\
		usage.c

OBJS=		$(SRCS:.c=.o)

LPLIBS=		$(LIBACC)	\
		$(LIBCLS)	\
		$(LIBMSG)	\
		$(LIBPRT)	\
		$(LIBFRM)	\
		$(LIBREQ)	\
		$(LIBOAM)	\
		$(LIBLP)

SYSLIBS=	-lcurses

LDLIBS +=	$(LPLIBS) $(SYSLIBS) $(I18N)

PROG=		lpadmin


#ROOTSYMLINK=	$(ROOTLIBPROG)

.KEEP_STATE:

all:		$(PROG)

$(PROG):	$(OBJS)
		$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

install:	all $(ROOTLIBLPLOCLPROG) #$(ROOTSYMLINK)

#$(ROOTSYMLINK):
#		$(RM) $@; $(SYMLINK) ../sbin/$(PROG) $@

_msg:
		@echo "Messages are made in usr/src/cmd/lp"

clean:
		$(RM) $(OBJS)

clobber: clean
		-$(RM) $(PROG) $(CLOBBERFILES)

strip:
		$(STRIP) $(PROG)

lint:
		$(LINT.c) $(SRCS) $(LDLIBS)
