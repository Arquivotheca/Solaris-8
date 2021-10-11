#
#ident	"@(#)Makefile	1.1	98/07/26 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# ucbcmd/ps/sparcv9/Makefile
#

PROG=	ps

OBJS=	ps.o

SRCS=	$(OBJS:%.o=../%.c)

include ../../Makefile.ucbcmd
include ../../Makefile.ucbcmd.64

LDLIBS +=	-lelf

CFLAGS64	+= -v

lint	:= LINTFLAGS64 = -x -D__sparcv9

FILEMODE= 4555
OWNER= root
GROUP= sys

.KEEP_STATE:

%.o:	../%.c
	$(COMPILE.c) $<

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG64)

clean:
	$(RM) $(OBJS)

lint:
	$(LINT.c) $(SRCS) $(LDLIBS)

include ../../Makefile.ucbtarg
