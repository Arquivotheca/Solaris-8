#
# Copyright (c) 1993, by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.7	93/12/03 SMI"
#

PROG=		sxcmem
SHFILES=	$(PROG)

include ../../../../../Makefile.cmd

PRIOCMEM=	00
PRIOFB=		91

STARTCMEM= $(ROOTETC)/rcS.d/S$(PRIOCMEM)$(PROG)

INITD= $(ROOTETC)/init.d
DIRS= $(INITD) $(ROOTETC)/rcS.d
FILEMODE= 0544
OWNER= root
GROUP= sys

ETCINITPROG=	$(SHFILES:%=$(INITD)/%)

.KEEP_STATE:

all: $(SHFILES)

install: all $(DIRS) $(ETCINITPROG) ln_PROG

$(INITD)/% : %
	$(INS.file)

ln_PROG :
	-$(RM) $(STARTCMEM)
	$(LN) $(INITD)/$(PROG) $(STARTCMEM)

$(DIRS):
	$(INS.dir)

clean:

lint:

include ../../../../../Makefile.targ
