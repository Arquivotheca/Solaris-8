#
# ident	"@(#)Makefile	1.2	92/08/12 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/lp/filter/postscript/common/Makefile
#

include		../../../Makefile.lp

SRCS =		glob.c misc.c request.c tempnam.c

OBJS =		$(SRCS:%.c=%.o)

TXTS =		README

ENCODING=	2
CPPFLAGS =	-DDFLTENCODING=$(ENCODING)	\
		-DSYSV				\
		-I. 				\
		$(CPPFLAGS.master)

.KEEP_STATE :

all :		$(TXTS) $(OBJS)

install strip :

clean :
		$(RM) $(OBJS)

lint :		lint_SRCS

clobber: clean

lint_SRCS:
	$(LINT.c) $(SRCS)
