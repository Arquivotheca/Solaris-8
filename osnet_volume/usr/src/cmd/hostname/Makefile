#
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.4	96/08/27 SMI"

PROG= hostname

include ../Makefile.cmd

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROG)

clean:

lint:

# This target "hand-crafts" a msgfile for hostname. If you
# add any stuff to the script, you may need to add messages
# here as well.
$(POFILE):
	$(RM) $@
	$(ECHO) 'msgid  "Usage: hostname [name]\\n"' > $@
	$(ECHO) 'msgstr' >> $@

include ../Makefile.targ
