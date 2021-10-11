#
#ident	"@(#)Makefile	1.10	97/03/13 SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG=du
XPG4PROG= du

include ../Makefile.cmd

CFLAGS += -v
$(XPG4) := CFLAGS += -DXPG4
CPPFLAGS += -D_FILE_OFFSET_BITS=64
POFILE = du_cmd.po
POFILES = du.po
POFILES_XPG4 = du.po.xpg4
$(POFILES_XPG4)	:= CFLAGS += -DXPG4
CLOBBERFILES += $(POFILES_XPG4)

.KEEP_STATE:

all: $(PROG) $(XPG4)

install: all $(ROOTPROG) $(ROOTXPG4PROG)

$(POFILE):	$(POFILES) $(POFILES_XPG4)
	$(RM) $@; cat $(POFILES) $(POFILES_XPG4) > $@

%.po.xpg4:	%.c
	$(COMPILE.cpp) $< > $<.i
	$(BUILD.po)

clean:

lint:	lint_PROG

include ../Makefile.targ
