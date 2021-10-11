#
#ident	"@(#)Makefile	1.9	96/03/19 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
#
# cmd/nl/Makefile
#
# Message catalog requires no special processing because messages
# only occur in ./nl.c, not in ../expr/compile.c.

PROG= nl
XPG4PROG= nl

NLOBJ= nlobjs/$(PROG).o nlobjs/compile.o
XPG4NLOBJ= nlobjs.xpg4/$(PROG).o nlobjs.xpg4/compile.o

OBJS = $(NLOBJ) $(XPG4NLOBJ)
SRCS = nl.c ../expr/compile.c

include ../Makefile.cmd

LDFLAGS = -s 
LDLIBS += -lgen
CFLAGS += -v
$(XPG4):= CPPFLAGS += -DXPG4

.KEEP_STATE:

all: $(PROG) $(XPG4)

install: all $(ROOTPROG) $(ROOTXPG4PROG)

$(PROG): nlobjs $(NLOBJ)
	$(LINK.c) $(NLOBJ) -o $@ $(LDLIBS)
	$(POST_PROCESS)

$(XPG4): nlobjs.xpg4 $(XPG4NLOBJ)
	$(LINK.c) $(XPG4NLOBJ) -o $@ $(LDLIBS)
	$(POST_PROCESS)

nlobjs/%.o:   %.c
	$(COMPILE.c) -o $@ $<

nlobjs/%.o:   ../expr/%.c
	$(COMPILE.c) -o $@ $<

nlobjs.xpg4/%.o:      %.c
	$(COMPILE.c) -o $@ $<

nlobjs.xpg4/%.o:      ../expr/%.c
	$(COMPILE.c) -o $@ $<

nlobjs:
	-@mkdir -p $@

nlobjs.xpg4:
	-@mkdir -p $@

clean:
	$(RM) $(OBJS)

lint:	
	lint_SRCS

include ../Makefile.targ

