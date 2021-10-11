#
# ident	"@(#)Makefile	1.12	99/08/16 SMI"	/* SVr4.0 1.13	*/
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/lp/lib/access/Makefile
#

LIBRARY	=	liblpacc.a

OBJECTS =	allowed.o	\
		bang.o		\
		change.o	\
		dumpaccess.o	\
		files.o		\
		loadaccess.o

include		../../../../lib/Makefile.lib
include		../../Makefile.lp

CPPFLAGS =	-I../../include $(CPPFLAGS.master)


.KEEP_STATE :

all install :	$(LIBS) 

_msg :
	@echo "Messages are made in usr/src/cmd/lp"

include		../../../../lib/Makefile.targ

CLEANFILES	+= llib-llpacc.ln
LINTFLAGS = -nvx
SRCS= $(OBJECTS:%.o=%.c)

lint:	lintlib
	$(LINT.c) $(LINTFLAGS) $(SRCS) 

lintlib:
	$(LINT.c) $(LINTFLAGS) -o lpacc llib-llpacc 

