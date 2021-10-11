#
#ident	"@(#)Makefile	1.2	99/08/25 SMI"
#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.

PROG=	protocmp
OBJS=	protocmp.o proto_list.o protodir.o list.o \
	exception_list.o arch.o depend.o
SRCS=	$(OBJS:%.o=%.c)
CLEANFILES += $(OBJS)

include ../Makefile.tools

OWNER=	root
GROUP=	bin
CFLAGS += -v

.KEEP_STATE:

all:	$(PROG)

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

install: all .WAIT $(ROOTONBLDMACHPROG)

lint:	lint_SRCS

clean:
	$(RM) $(CLEANFILES)

include ../Makefile.targ
