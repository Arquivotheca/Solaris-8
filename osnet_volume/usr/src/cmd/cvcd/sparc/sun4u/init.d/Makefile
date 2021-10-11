#
# ident	"@(#)Makefile	1.3	99/12/06 SMI"
#
# Copyright (c) 1997-1998 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG=		cvc
SHFILES=	$(PROG)
SHSRCS=		$(PROG:%=%.sh)

include ../../../../Makefile.cmd

PRIOCVC=	10

STARTCVC= $(ROOTETC)/rcS.d/S$(PRIOCVC)$(PROG)

INITD= $(ROOTETC)/init.d
DIRS= $(INITD) $(ROOTETC)/rcS.d
FILEMODE= 0744
OWNER= root
GROUP= sys

ETCINITPROG=	$(SHFILES:%=$(INITD)/%)

.KEEP_STATE:

all: $(SHFILES)

install: all .WAIT $(DIRS) .WAIT $(ETCINITPROG) .WAIT ln_PROG

$(INITD)/% : %
	$(INS.file)

ln_PROG :
	-$(RM) $(STARTCVC)
	$(LN) $(INITD)/$(PROG) $(STARTCVC)

$(DIRS):
	$(INS.dir)

clean:

lint:

include ../../../../Makefile.targ
