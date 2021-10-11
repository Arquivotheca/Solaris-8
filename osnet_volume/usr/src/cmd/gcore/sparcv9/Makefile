#
#ident	"@(#)Makefile	1.3	99/03/23 SMI"
#
# Copyright (c) 1989, 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/gcore/sparcv9/Makefile
#

PROG=	gcore

OBJS=	gcore.o elf_notes.o old_notes.o

SRCS=	$(OBJS:%.o=../%.c)

include ../../Makefile.cmd
include ../../Makefile.cmd.64

CFLAGS64	+= -v
CPPFLAGS	+= -D_ELF64

LDLIBS	+= -lproc

lint	:= LINTFLAGS64 = -x -Xarch=v9

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

include ../../Makefile.targ
