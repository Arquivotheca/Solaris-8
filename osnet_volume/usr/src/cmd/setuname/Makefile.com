#
#ident	"@(#)Makefile.com	1.1	97/05/30 SMI"
#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/setuname/Makefile.com
#

PROG= 	setuname
OBJS=	$(PROG).o
SRCS=	$(OBJS:%.o=../%.c)

include ../../Makefile.cmd

CFLAGS +=	-v
CFLAGS64 +=	-v
LDLIBS +=	-lkvm -lelf

CLEANFILES	+= $(OBJS)

.KEEP_STATE:

all: $(PROG) 

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

lint:	lint_SRCS

%.o:	../%.c
	$(COMPILE.c) $<

clean:
	$(RM) $(CLEANFILES)

include ../../Makefile.targ
