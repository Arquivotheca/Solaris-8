#
# ident	"@(#)Makefile	1.12	99/08/16 SMI"	/* SVr4.0 1.13	*/
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/lp/lib/requests/Makefile
#

LIBRARY	=	liblpreq.a

OBJECTS =	anyrequests.o	\
		freerequest.o	\
		getrequest.o	\
		r_head.o	\
		putrequest.o


include		../../../../lib/Makefile.lib
include         ../../Makefile.lp

CPPFLAGS =	-I../../include $(CPPFLAGS.master) -K pic

.KEEP_STATE:

all install :	$(LIBS)

_msg:
	@echo "Messages are made in usr/src/cmd/lp"

include		../../../../lib/Makefile.targ

CLEANFILES	+= llib-llpreq.ln
LINTFLAGS = -nvx
SRCS= $(OBJECTS:%.o=%.c)
LINT_CPPFLAGS =	-I../../include $(CPPFLAGS.master)

lint:	lintlib
	$(LINT) $(LINTFLAGS) $(LINT_CPPFLAGS) $(SRCS)

lintlib:
	$(LINT) $(LINTFLAGS) $(LINT_CPPFLAGS) -o lpreq llib-llpreq
