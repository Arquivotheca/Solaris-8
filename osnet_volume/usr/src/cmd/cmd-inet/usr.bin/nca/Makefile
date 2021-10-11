#
#ident	"@(#)Makefile	1.1	99/08/06 SMI"	
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
# 

PROG=	ncab2clf 
OBJS=	ncab2clf.o
SRCS=	$(OBJS:%.o=%.c)

include	../../../Makefile.cmd

#
# Message catalog
#
POFILE=	ncab2clf.po
#

INC_PATH +=	../../../../uts/common/inet/nca -I.
CFLAGS +=	-v
CPPFLAGS +=	-I$(INC_PATH)

.KEEP_STATE:

.PARALLEL: $(OBJS)

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@
	$(POST_PROCESS)

#
# Message catalog
#
_msg: $(POFILE)

$(POFILE): $(SRCS)
	$(RM) ncab2clf.po
	$(COMPILE.cpp) $(SRCS) > $(POFILE).i
	$(XGETTEXT) $(XGETFLAGS)	$(POFILE).i
	sed "/^domain/d" messages.po > $@
	$(RM) messages.po $(POFILE).i

#
install: all $(ROOTPROG)

clean:
	$(RM) $(OBJS)

lint:	lint_SRCS

include ../../../Makefile.targ
