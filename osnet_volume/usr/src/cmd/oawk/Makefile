#
# ident	"@(#)Makefile	1.19	96/04/20 SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/oawk/Makefile
#

include		../Makefile.cmd

PROG =		oawk
LINKPROG =	awk

SRCS =		b.c lib.c main.c parse.c run.c tran.c

#TXTS =		README EXPLAIN

OBJS =		awk.g.o awk.lx.o proctab.o tmptoken.o		\
		$(SRCS:%.c=%.o)

#
# for message catalogue
#
POFILES=        $(OBJS:%.o=%.po)
POFILE=         oawk.po
XGETFLAGS +=    -a -x oawk.xcl

NATIVEDIR =	native
MAKEPRCTAB =	$(NATIVEDIR)/makeprctab

NATIVEOBJS =	$(NATIVEDIR)/makeprctab.o $(NATIVEDIR)/tmptoken.o
$(MAKEPRCTAB) :=	CC = $(NATIVECC)
$(MAKEPRCTAB) :=	POST_PROCESS=
$(MAKEPRCTAB) :=	POST_PROCESS_O=

CLEANFILES =	proctab.c y.tab.h y.tab.c awk.h awk.g.c		\
		tmptoken.c awk.lx.c $(NATIVEOBJS) $(MAKEPRCTAB)	\
		temp

YFLAGS =	-d
XLDLIBS5CC +=	-lm 
LDLIBS +=	-lm
CPPFLAGS =      -I. $(CPPFLAGS.master)
CPPFLAGS +=	-D_FILE_OFFSET_BITS=64

ROOTLINK =	$(LINKPROG:%=$(ROOTBIN)/%)

.KEEP_STATE :

all :		$(PROG) $(TXTS)

$(PROG) :	$(OBJS)
		$(LINK.c) $(OBJS)  -o $@  $(LDLIBS)
		$(POST_PROCESS)

$(POFILE):      $(POFILES)
	$(RM)	$@
	cat     $(POFILES)      > $@

awk.g.c + awk.h :	awk.g.y
		$(RM) awk.g.c awk.h
		$(YACC.y) awk.g.y
		mv y.tab.c awk.g.c
		mv y.tab.h awk.h

awk.lx.c:	awk.lx.l
		lex  -wt awk.lx.l > awk.lx.c

tmptoken.c :	tokenscript token.c
		$(RM) $@
		ed - < tokenscript

proctab.c :	$(MAKEPRCTAB)
		$(RM) $@
		$(MAKEPRCTAB) > $@

$(MAKEPRCTAB) :	$(NATIVEDIR) $(NATIVEOBJS)
		$(LINK.c) $(NATIVEOBJS) -o $@ $(XLDLIBS5CC)

install : 	all $(ROOTPROG) $(ROOTLINK)

$(NATIVEDIR) :
		-@mkdir -p $(NATIVEDIR)

$(NATIVEDIR)/%.o :	%.c
		$(COMPILE.c) -o $@ $<

$(ROOTLINK) :	$(ROOTPROG)
		$(RM) $@; $(LN) $(ROOTPROG) $@

clean:
		$(RM) $(OBJS) $(CLEANFILES)

lint :		lint_SRCS

strip:
		$(STRIP) $(PROG)

include		../Makefile.targ
