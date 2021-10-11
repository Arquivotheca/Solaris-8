#
# ident	"@(#)Makefile	1.13	99/08/16 SMI"	/* SVr4.0 1.13	*/
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/lp/lib/msgs/Makefile
#

LIBRARY	=	liblpmsg.a

OBJECTS =	_getmessage.o	\
		_putmessage.o	\
		getmessage.o	\
		hslconv.o	\
		putmessage.o	\
		msgfmts.o	\
		mclose.o	\
		mconnect.o	\
		mdisconnect.o	\
		mgetputm.o	\
		mlisten.o	\
		mcreate.o	\
		mdestroy.o	\
		mneeds.o	\
		mopen.o		\
		mread.o		\
		mrecv.o		\
		msend.o		\
		mwrite.o	\
		streamio.o	\
		fifo_buffs.o	\
		read_fifo.o	\
		write_fifo.o


include		../../../../lib/Makefile.lib
include         ../../Makefile.lp


CPPFLAGS =	-I../../include $(CPPFLAGS.master) -K pic

.KEEP_STATE:

all install :	$(LIBS)

_msg :
	@echo "Messages are made in usr/src/cmd/lp"

include		../../../../lib/Makefile.targ

CLEANFILES	+= llib-llpmsg.ln
LINTFLAGS = -nvx
SRCS= $(OBJECTS:%.o=%.c)
LINT_CPPFLAGS =	-I../../include $(CPPFLAGS.master) 

lint:	lintlib
	$(LINT) $(LINTFLAGS) $(LINT_CPPFLAGS) $(SRCS)

lintlib:
	$(LINT) $(LINTFLAGS) $(LINT_CPPFLAGS) -o lpmsg llib-llpmsg
