#
#ident	"@(#)Makefile	1.10	96/04/20 SMI"
#
# Copyright (c) 1994, 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/chmod/Makefile
#

PROG= chmod
OBJS= chmod.o common.o
SRCS= $(OBJS:%.o=%.c)

POFILES= $(OBJS:%.o=%.po)

include ../Makefile.cmd

CFLAGS += -v

CPPFLAGS += -D_FILE_OFFSET_BITS=64

POFILE= chmod_cmd.po
XGETFLAGS= -a -x chmod.xcl

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

lint:	lint_SRCS

include ../Makefile.targ
