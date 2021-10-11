#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.5	99/11/19 SMI"
#

PROG=	prtconf
OBJS=	$(PROG).o pdevinfo.o prt_xxx.o
SRCS=	$(OBJS:%.o=../%.c)

include ../../Makefile.cmd

CFLAGS	+=	-v
#CFLAGS64 +=	-v
LDLIBS	+= -ldevinfo

OWNER= root
GROUP= sys
FILEMODE= 02555

LINTFLAGS= -x
LINTFLAGS64= -x -Xarch=v9

CLEANFILES += $(OBJS)

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

