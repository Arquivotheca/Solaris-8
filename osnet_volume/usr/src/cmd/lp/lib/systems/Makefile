#
# ident	"@(#)Makefile	1.10	99/08/16 SMI"	/* SVr4.0 1.13	*/
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/lp/lib/systems/Makefile
#

LIBRARY	=	liblpsys.a

OBJECTS =	delsystem.o \
		freesystem.o \
		getsystem.o \
		putsystem.o


include		../../../../lib/Makefile.lib
include         ../../Makefile.lp

CPPFLAGS =	-I../../include $(CPPFLAGS.master)

.KEEP_STATE:

SRCS= $(OBJECTS:%.o=%.c)

all install :	$(LIBS)

_msg :
	@echo "Messages are made in usr/src/cmd/lp"

lint :

include		../../../../lib/Makefile.targ
