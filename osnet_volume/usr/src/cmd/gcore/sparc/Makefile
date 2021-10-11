#
#ident	"@(#)Makefile	1.2	97/12/23 SMI"
#
# Copyright (c) 1989,1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/gcore/sparc/Makefile
#

PROG=	gcore

OBJS=	gcore.o elf_notes.o old_notes.o

SRCS=	$(OBJS:%.o=../%.c)

include ../../Makefile.cmd

CFLAGS	+= -v

LDLIBS	+= -lproc

.KEEP_STATE:

lint	:= LINTFLAGS = -x

%.o:	../%.c
	$(COMPILE.c) $<

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG32)

clean:
	$(RM) $(OBJS)

lint:
	$(LINT.c) $(SRCS) $(LDLIBS)

include ../../Makefile.targ
