#
#ident	"@(#)Makefile	1.19	96/07/26 SMI"	/* SVr4.0 1.1	*/
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#
#	cmd/src/localdef/colltbl/Makefile

include  ../../Makefile.cmd

PROG=	colltbl
# anything related to NPROG should be removed

OBJS=	colltbl.o collfcns.o diag.o parse.o lex.o utils.o source.o
SRCS=	$(OBJS:%.o=%.c)

LDLIBS += -ly

CLEANFILES=	$(OBJS)  parse.c 


.KEEP_STATE:


all:	$(PROG)

install: all $(ROOTPROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)


lint: lint_SRCS

parse.c:	parse.y
	$(YACC.y) -d parse.y
	$(MV) y.tab.c parse.c

clean:
	-$(RM) $(CLEANFILES)

include ../../Makefile.targ
