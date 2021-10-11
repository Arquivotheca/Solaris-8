#
# Copyright (c) 1993, 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/asppp/etc/Makefile
#
#ident	"@(#)Makefile	1.14	97/12/08 SMI"

include ../../Makefile.cmd

ETCFILES = asppp.cf
PROG = asppp
ROOTINIT_D = $(ROOTETC)/init.d
ROOTRC0_D = $(ROOTETC)/rc0.d
ROOTRCS_D = $(ROOTETC)/rcS.d
ROOTRC1_D = $(ROOTETC)/rc1.d
ROOTRC2_D = $(ROOTETC)/rc2.d

OWNER = root
GROUP = sys
FILEMODE = 0744

ROOTETCFILES= $(ETCFILES:%=$(ROOTETC)/%)

ROOTINIT_DPROG = $(ROOTINIT_D)/$(PROG)

ROOTLNKPROGS = 	$(ROOTRC0_D)/K50asppp \
		$(ROOTRCS_D)/K50asppp \
		$(ROOTRC1_D)/K50asppp \
		$(ROOTRC2_D)/S47asppp

.KEEP_STATE:

all:	$(PROG) $(ETCFILES)

install: $(ROOTETCFILES) $(ROOTINIT_DPROG) $(ROOTLNKPROGS)

$(ROOTINIT_DPROG): $(PROG)
	$(INS.file) $(PROG)

clean:

clobber: 

$(ROOTLNKPROGS):	$(ROOTINIT_DPROG)
	$(RM) $@; $(LN) $(ROOTINIT_DPROG) $@



