#
#ident	"@(#)Makefile.com	1.1	97/08/06 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/tnf/prex/Makefile
#

PROG=		prex

OBJS.c=		source.o	\
		main.o		\
		util.o		\
		expr.o		\
		spec.o		\
		set.o		\
		queue.o		\
		cmd.o		\
		new.o		\
		list.o		\
		fcn.o		\
		prbk.o		\
		help.o

OBJS.yl=	prexgram.o	\
		prexlex.o

OBJS=		 $(OBJS.yl) $(OBJS.c)

SRCS= $(OBJS.c:%.o=../%.c) $(OBJS.yl:%.o=%.c)

include	../../../Makefile.cmd

POFILE= prex.po
POFILES= $(OBJS.c:%.o=%.po)

#YFLAGS=	-d -t -v
YFLAGS=		-d
LFLAGS=		-v
# FOR normal makefile, uncomment the next line
LDLIBS +=	-lgen -ltnfctl -lelf -lc
# Uncomment the following line for a debug build
# COPTFLAG =	-g -DDEBUG -v

.KEEP_STATE:

.PARALLEL: $(OBJS)

all: $(PROG)

$(PROG): $(OBJS)
	$(RM) ../prexlex.c ../prexgram.c
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

%.o:	../%.c
	$(COMPILE.c) $<

help.o: y.tab.h

y.tab.h: prexgram.o

$(ROOTBIN):
	$(INS.dir)

$(POFILE):      $(POFILES)
	$(RM)	$@
	cat     $(POFILES)      > $@

clean:
	$(RM) $(OBJS) y.tab.h prexlex.c prexgram.c ../prexlex.c ../prexgram.c

#lint: $(SRCS) lint_SRCS
lint: $(OBJS) 
	$(MV) ../prexlex.c .
	$(MV) ../prexgram.c .
	$(LINT.c) $(SRCS)

include	../../../Makefile.targ
