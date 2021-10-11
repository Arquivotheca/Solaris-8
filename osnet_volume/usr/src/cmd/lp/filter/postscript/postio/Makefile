#
# ident	"@(#)Makefile	1.8	94/03/30 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/lp/filter/postscript/postio/Makefile
#

include         ../../../Makefile.lp

COMMONDIR=      ../common

PROG=           postio

SRCS=		postio.c ifdef.c slowsend.c parallel.c

OBJS =		$(SRCS:%.c=%.o)

TXTS=		README 

ENCODING=	2
CPPFLAGS =	-DDFLTENCODING=$(ENCODING)	\
		-DSYSV				\
		-I. -I$(COMMONDIR)		\
		$(CPPFLAGS.master)

.KEEP_STATE:

all:		$(TXTS) $(PROG)

install:	all $(ROOTLIBLPPOSTPROG)

$(PROG):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

clean :
		$(RM) $(OBJS)

strip:
		$(STRIP) $(PROG)

lint:		lint_SRCS

include		../../../../Makefile.targ
