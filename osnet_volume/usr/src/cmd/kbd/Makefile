#
#ident	"@(#)Makefile	1.4	96/06/10 SMI"
#
# Copyright (c) 1991, 1996 by Sun Microsystems, Inc.
# All Rights Reserved.
#

include ../Makefile.cmd

.KEEP_STATE:

PROG	= kbd
CFLAGS	+= -v
LDLIBS	+= -lcmd

DFLTD	= $(ROOTETC)/default
DDIRS	= $(DFLTD)

ETCDFLTPROG = $(PROG:%=$(DFLTD)/%)
$(ETCDFLTPROG) := FILEMODE = 0444
$(ETCDFLTPROG) := OWNER = root
$(ETCDFLTPROG) := GROUP = sys

all: $(PROG) $(PROG).dfl

install: all $(DDIRS) $(ETCDFLTPROG) $(ROOTPROG)

$(DDIRS):
	$(INS.dir)

$(DFLTD)/% : %.dfl
	$(INS.rename)

clean:
	$(RM) $(PROG)

lint:	lint_PROG

include ../Makefile.targ
