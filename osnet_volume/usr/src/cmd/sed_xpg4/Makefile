#
#ident	"@(#)Makefile	1.6	96/04/20 SMI"
#
# Copyright (c) 1995,1996 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= sed
XPG4PROG= sed

OBJS= sed0.o sed1.o
SRCS= $(OBJS:%.o=%.c)

include ../Makefile.cmd

CFLAGS += -v
CPPFLAGS += -D_FILE_OFFSET_BITS=64

# for messaging catalog
POFILE= sed_xpg4.po
POFILES= $(OBJS:%.o=%.po)
XGETFLAGS += -a -x sed.xcl


.KEEP_STATE:

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

lint: lint_SRCS

include ../Makefile.targ
