#
#ident	"@(#)Makefile 1.6	96/04/20 SMI"
#
# Copyright (c), 1995,1996 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= more
XPG4PROG= more
OBJS= ch.o command.o decode.o help.o input.o line.o linenum.o main.o \
      option.o os.o output.o position.o prim.o screen.o signal.o tags.o \
      ttyin.o
SRCS= $(OBJS:%.o=%.c)
DATA= more.help

POFILES= $(OBJS:%.o=%.po)

include ../Makefile.cmd

CFLAGS += -v
LDLIBS += -lcmd -lcurses
CPPFLAGS += -D_FILE_OFFSET_BITS=64

POFILE=more_xpg4.po
XGETFLAGS= -a -x more.xcl

.KEEP_STATE:

.PARALLEL: $(OBJS)

all: $(XPG4)

$(XPG4): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

$(POFILE): $(POFILES)
	$(RM) $@
	cat $(POFILES) > $@

install: all $(ROOTXPG4PROG)

clean:
	$(RM) $(OBJS)

lint:	lint_SRCS

include ../Makefile.targ
