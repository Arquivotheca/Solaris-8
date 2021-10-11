#
# ident	"@(#)Makefile	1.14	99/08/16 SMI"	/* SVr4.0 1.13	*/
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/lp/lib/lp/Makefile
#

LIBRARY	=	liblp.a

OBJECTS =	Syscalls.o	\
		Sys_malloc.o	\
		addlist.o	\
		addstring.o	\
		appendlist.o	\
		alerts.o	\
		charset.o	\
		cs_strcmp.o	\
		cs_strncmp.o	\
		dellist.o	\
		dashos.o	\
		dirs.o		\
		duplist.o	\
		files.o		\
		freelist.o	\
		getlist.o	\
		getname.o	\
		getpaths.o	\
		getspooldir.o	\
		isterminfo.o	\
		joinlist.o	\
		lenlist.o	\
		lp_errno.o	\
		makepath.o	\
		makestr.o	\
		mergelist.o	\
		next.o		\
		printlist.o	\
		sdn.o		\
		sprintlist.o	\
		searchlist.o	\
		set_charset.o	\
		set_pitch.o	\
		set_size.o	\
		sop.o		\
		strip.o		\
		syntax.o	\
		tidbit.o	\
		tinames.o	\
		wherelist.o	\
		which.o


include		../../../../lib/Makefile.lib
include         ../../Makefile.lp

CPPFLAGS =	-I../../include $(CPPFLAGS.master) -K pic

.KEEP_STATE:

all install :	$(LIBS) 

_msg :
	@echo "Messages are made in usr/src/cmd/lp"

include		../../../../lib/Makefile.targ

CLEANFILES	+= llib-llp.ln
LINTFLAGS = -nvx
SRCS= $(OBJECTS:%.o=%.c)
# lint does not take -K pic 
LINT_CPPFLAGS =	-I../../include $(CPPFLAGS.master)

lint:	lintlib
	$(LINT) $(LINTFLAGS) $(LINT_CPPFLAGS) $(SRCS)

lintlib:
	$(LINT) $(LINTFLAGS) $(LINT_CPPFLAGS) -o lp llib-llp
