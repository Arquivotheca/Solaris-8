#
#ident  "@(#)Makefile 1.4	00/09/14 SMI"
#
# Copyright (c) 1995, 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/awk_xpg4/Makefile
#

# NOTE: this is nawk in usr/src/cmd/awk_xpg4 to reside as /usr/xpg4/bin/awk

PROG= awk
XPG4PROG= awk

OBJ1= awk0.o awk1.o awk2.o awk3.o awk4.o
OBJ2= awk.o
OBJS= $(OBJ2) $(OBJ1)
SRCS= $(OBJ1:%.o=%.c)

include ../Makefile.cmd

CPPFLAGS += -D_FILE_OFFSET_BITS=64
CFLAGS += -v
YFLAGS += -d
LDLIBS += -lm
CLEANFILES= awk.c y.tab.h

# for messaging catalog

POFILE= awk_xpg4.po
POFILES= $(OBJS:%.o=%.po)

.KEEP_STATE:

all: $(XPG4)

$(XPG4): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

$(POFILE): $(POFILES)
	$(RM) $@
	cat $(POFILES) > $@
#

install: all $(ROOTXPG4PROG)

clean:
	$(RM) $(OBJS) $(CLEANFILES)

lint: lint_SRCS


include ../Makefile.targ
