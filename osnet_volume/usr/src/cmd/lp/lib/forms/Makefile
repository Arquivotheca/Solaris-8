#
# ident "@(#)Makefile 1.11     99/08/16 SMI"        /* SVr4.0 1.13  */
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/lp/lib/forms/Makefile
#

LIBRARY	=	liblpfrm.a

OBJECTS	=	delform.o	\
		f_head.o	\
		freeform.o	\
		getform.o	\
		putform.o	\
		rdform.o	\
		wrform.o


include		../../../../lib/Makefile.lib
include         ../../Makefile.lp

CPPFLAGS =	-I../../include $(CPPFLAGS.master)

.KEEP_STATE:

all install :	$(LIBS) 

_msg :
	@echo "Messages are made in usr/src/cmd/lp"

include		../../../../lib/Makefile.targ

CLEANFILES	+= llib-llpfrm.ln
LINTFLAGS = -nvx
SRCS= $(OBJECTS:%.o=%.c)

lint:	lintlib
	$(LINT.c) $(LINTFLAGS) $(SRCS)

lintlib:
	$(LINT.c) $(LINTFLAGS) -o lpfrm llib-llpfrm
