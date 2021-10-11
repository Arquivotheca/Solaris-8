#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/llc2/etc/Makefile
#
#ident	"@(#)Makefile	1.3	98/10/26 SMI"

include ../../Makefile.cmd

ETCFILES = llc2_start.default
PROG = llc2
ROOTINIT_D = $(ROOTETC)/init.d
ROOTRC0_D = $(ROOTETC)/rc0.d
ROOTRC1_D = $(ROOTETC)/rc1.d
ROOTRC2_D = $(ROOTETC)/rc2.d
ROOTRCS_D = $(ROOTETC)/rcS.d

OWNER = root
GROUP = sys
FILEMODE = 0744

ROOTETCDIR	=	$(ROOTETC)/llc2
ROOTETCDEFDIR	= 	$(ROOTETC)/llc2/default
ROOTETCFILES	= 	$(ETCFILES:%=$(ROOTETC)/llc2/%)

$(ROOTETCDIR):
	$(INS.dir)

$(ROOTETCDEFDIR):
	$(INS.dir)

$(ROOTETC)/llc2/%: %
	$(INS.file)

ROOTINIT_DPROG = $(ROOTINIT_D)/$(PROG)

ROOTLNKPROGS = 	$(ROOTRC0_D)/K52llc2 \
		$(ROOTRC1_D)/K52llc2 \
		$(ROOTRCS_D)/K52llc2 \
		$(ROOTRC2_D)/S40llc2

.KEEP_STATE:

all:	$(PROG) $(ETCFILES)

install: $(ROOTETCDIR) $(ROOTETCDEFDIR) $(ROOTETCFILES) $(ROOTINIT_DPROG) \
	$(ROOTLNKPROGS)

$(ROOTINIT_DPROG): $(PROG)
	$(INS.file) $(PROG)

clean:

clobber: 

$(ROOTLNKPROGS):	$(ROOTINIT_DPROG)
	$(RM) $@; $(LN) $(ROOTINIT_DPROG) $@
