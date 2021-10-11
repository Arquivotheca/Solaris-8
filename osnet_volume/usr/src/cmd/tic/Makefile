#
#ident	"@(#)Makefile	1.1	97/08/28 SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= tic

OBJS= \
	tic_captab.o \
	tic_error.o \
	tic_hash.o \
	tic_main.o \
	tic_parse.o \
	tic_read.o \
	tic_scan.o

SRCS= $(OBJS:%.o=%.c)

include ../Makefile.cmd

LDLIBS += -lcurses
CPPFLAGS += -I../../lib/libcurses/screen

.KEEP_STATE:

all: $(PROG) 

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

$(POFILE): $(POFILES)
	$(RM) $@
	cat $(POFILES) > $@


install: all $(ROOTPROG)

clean:
	$(RM) $(OBJS)

lint: lint_SRCS

include ../Makefile.targ
