#
#ident  "@(#)Makefile 1.1     94/06/08 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= utmpd

include ../Makefile.cmd

DFLTD= $(ROOTETC)/default
DIRS= $(DFLTD)

ETCDFLTPROG = $(PROG:%=$(DFLTD)/%)
$(ETCDFLTPROG) := FILEMODE = $(LIBFILEMODE)
$(ETCDFLTPROG) := GROUP = sys

FILEMODE = 555
OWNER = root
GROUP = bin
LDLIBS += -lcmd

.KEEP_STATE:

all: $(PROG) 

install: all $(DIRS) $(ROOTLIBPROG) $(PROG).dfl $(ETCDFLTPROG)

$(DFLTD)/% : %
	$(MV) $(PROG) $(PROG).save
	$(LN) $(PROG).dfl $(PROG)
	$(INS.file)
	$(RM) $(PROG)
	$(MV) $(PROG).save $(PROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
