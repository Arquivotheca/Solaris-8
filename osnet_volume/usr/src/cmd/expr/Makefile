#
#ident	"@(#)Makefile	1.14	96/10/13 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All Rights Reserved

PROG= expr
XPG4PROG= expr

EXPROBJ= exprobjs/expr.o exprobjs/compile.o
XPG4EXPROBJ= exprobjs.xpg4/expr.o exprobjs.xpg4/compile.o

OBJS= $(EXPROBJ) $(XPG4EXPROBJ)
SRCS= expr.c compile.c

include ../Makefile.cmd

CFLAGS += -v
$(XPG4) := CFLAGS += -DXPG4

# This flag is being added only for SCO (x86) compatibility
CFLAGS += $(iBCS2FLAG)

XGETFLAGS += -a -x expr.xcl

LDLIBS += -lgen

.KEEP_STATE:

all: $(PROG) $(XPG4)

install: all $(ROOTPROG) $(ROOTXPG4PROG)

$(PROG): exprobjs $(EXPROBJ)
	$(LINK.c) $(EXPROBJ) -o $@ $(LDLIBS)
	$(POST_PROCESS)

$(XPG4): exprobjs.xpg4 $(XPG4EXPROBJ)
	$(LINK.c) $(XPG4EXPROBJ) -o $@ $(LDLIBS)
	$(POST_PROCESS)

exprobjs/%.o:	%.c
	$(COMPILE.c) -o $@ $<

exprobjs.xpg4/%.o:	%.c
	$(COMPILE.c) -o $@ $<

exprobjs:
	-@mkdir -p $@

exprobjs.xpg4:
	-@mkdir -p $@

clean:
	$(RM) $(OBJS)

lint:	lint_PROG

include ../Makefile.targ
