#
# ident	"@(#)Makefile	1.11	99/08/16 SMI"	/* SVr4.0 1.13	*/
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/lp/lib/filters/Makefile
#

LIBRARY	=	liblpflt.a

OBJECTS =	conv.o		\
		delfilter.o	\
		dumpfilters.o	\
		freefilter.o	\
		getfilter.o	\
		filtertable.o	\
		insfilter.o	\
		loadfilters.o	\
		putfilter.o	\
		regex.o		\
		search.o	\
		trash.o

include         ../../Makefile.lp

include		../../../../lib/Makefile.lib

CPPFLAGS =	-I../../include $(CPPFLAGS.master)

.KEEP_STATE:

_msg :
	@echo "Messages are made in usr/src/cmd/lp"

all install :	$(LIBS)

include		../../../../lib/Makefile.targ

CLEANFILES	+= llib-llpflt.ln
LINTFLAGS = -nvx
SRCS= $(OBJECTS:%.o=%.c)

lint:	lintlib
	$(LINT.c) $(LINTFLAGS) $(SRCS)

lintlib:
	$(LINT.c) $(LINTFLAGS) -o lpflt llib-llpflt
