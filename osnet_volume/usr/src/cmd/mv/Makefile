#
#ident	"@(#)Makefile	1.17	99/02/05 SMI"
#
# Copyright (c) 1989,1995,1996-1999 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= mv
XPG4PROG= mv
STATPROG= mv
OBJS= mv.o
CPFILE= cp
LNFILE= ln
ROOTLINKS= $(ROOTBIN)/$(CPFILE) $(ROOTBIN)/$(LNFILE)
ROOTSTATLINKS= $(ROOTUSRSBINSTAT)/$(CPFILE) $(ROOTUSRSBINSTAT)/$(LNFILE)
ROOTXPG4LINKS= $(ROOTXPG4BIN)/$(CPFILE) $(ROOTXPG4BIN)/$(LNFILE)

include ../Makefile.cmd

clean $(XPG4) := OBJS += values-xpg4.o

CLOBBERFILES += $(CPFILE) $(LNFILE)
CFLAGS += -v
$(XPG4) := CFLAGS += -DXPG4
XGETFLAGS += -a -x mv.xcl
CPPFLAGS += -D_FILE_OFFSET_BITS=64

.KEEP_STATE:

all: $(PROG) $(CPFILE) $(LNFILE) $(STATIC) $(XPG4)

$(PROG) $(XPG4): $$(OBJS)
	$(LINK.c) -o $@ $(OBJS)
	$(POST_PROCESS)

$(CPFILE):	$(PROG)
		@$(RM) $(CPFILE); $(LN) $(PROG) $(CPFILE)

$(LNFILE):	$(PROG)
		@$(RM) $(LNFILE); $(LN) $(PROG) $(LNFILE)

install: all $(ROOTXPG4PROG) $(ROOTLINKS) $(ROOTSTATLINKS) $(ROOTXPG4LINKS)

$(ROOTLINKS):	$(ROOTPROG)
	$(RM) $@
	$(LN) $(ROOTPROG) $@

$(ROOTSTATLINKS):	$(ROOTSTATPROG)
	$(RM) $@
	$(LN) $(ROOTSTATPROG) $@

$(ROOTXPG4LINKS):	$(ROOTXPG4PROG)
	$(RM) $@
	$(LN) $(ROOTXPG4PROG) $@

values-xpg4.o: ../../lib/libc/port/gen/values-xpg4.c
	$(COMPILE.c) -o $@ ../../lib/libc/port/gen/values-xpg4.c

clean:
	$(RM) $(OBJS)

lint:	lint_PROG

include ../Makefile.targ
