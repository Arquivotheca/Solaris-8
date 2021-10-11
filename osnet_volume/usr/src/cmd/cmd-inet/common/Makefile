#
#pragma ident   "@(#)Makefile 1.2     99/03/30 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/common/Makefile

OBJS=		ifaddrlist.o 

SRCS=		$(OBJS:.o=.c)
HDRS=		ifaddrlist.h

include	../../Makefile.cmd


CPPFLAGS +=	-O -I. 

.KEEP_STATE:

.INIT:	$(HDRS)


all:	$(OBJS)

clean:	
	$(RM) $(OBJS) tags

lint:	lint_SRCS

install:	all

tags:	$(SRCS)
	ctags $(SRCS)

# When an internationalized code is added to this directory, _msg has to be
# defined in order to generate message catalog. Also, move 'common' directory
# from 'SUBDIR1' to 'SUBDIR2' in usr/src/cmd/cmd-inet/Makefile

include ../../Makefile.targ
