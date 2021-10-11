#
# ident	"@(#)Makefile	1.6	92/12/15 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/lp/filter/postscript/postmd/Makefile
#

include         ../../../Makefile.lp

PROG=           postmd

SRCS=		postmd.c

OBJS =		$(SRCS:%.c=%.o)

COMMONDIR =	../common

COMMONOBJS =	$(COMMONDIR)/request.o	\
		$(COMMONDIR)/glob.o	\
		$(COMMONDIR)/misc.o

TXTS=		README 

LDLIBS +=	-lm

ENCODING=	2
CPPFLAGS =	-DDFLTENCODING=$(ENCODING)	\
		-I. -I$(COMMONDIR)		\
		$(CPPFLAGS.master)

.KEEP_STATE:

all:		$(TXTS) $(PROG)

install:	all $(ROOTLIBLPPOSTPROG)

$(PROG):	$(OBJS) $(COMMONOBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS) $(COMMONOBJS)
		$(POST_PROCESS)

$(COMMONOBJS):	$$(@:%.o=%.c)
		cd $(@D); $(MAKE) $(@F)

clean:
		$(RM) $(OBJS)

strip:
		$(STRIP) $(PROG)

lint:		lint_SRCS

include		../../../../Makefile.targ
