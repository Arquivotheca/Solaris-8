#
# ident	"@(#)Makefile	1.6	94/03/30 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/lp/filter/postscript/picpack/Makefile
#

include		../../../Makefile.lp

PROG=		picpack

SRCS=		picpack.c

OBJS =		$(SRCS:%.c=%.o)

COMMONDIR =	../common

COMMONOBJS =	$(COMMONDIR)/glob.o	\
		$(COMMONDIR)/misc.o	\
		$(COMMONDIR)/tempnam.o


ENCODING =	2
CPPFLAGS =	-DDFLTENCODING=$(ENCODING)	\
		-I$(COMMONDIR)			\
		$(CPPFLAGS.master)

.KEEP_STATE:

all:		$(PROG)

install:	all $(ROOTLIBLPPOSTPROG)

$(PROG):	$(OBJS) $(COMMONOBJS)
		$(LINK.c) -o $@ $(OBJS) $(COMMONOBJS) $(LDLIBS)
		$(POST_PROCESS)

$(COMMONOBJS):	$$(@:%.o=%.c)
		cd $(@D); $(MAKE) $(@F)

clean :
		$(RM) $(OBJS)

strip:
		$(STRIP) $(PROG)

lint:		lint_SRCS

include		../../../../Makefile.targ
