#
# Copyright (c) 1996-1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/lp/etc/Makefile
#
#ident	"@(#)Makefile	1.11	97/12/08 SMI"

include			../Makefile.lp

ROOTINIT_D =		$(ROOTETC)/init.d
ROOTRC0_D =		$(ROOTETC)/rc0.d
ROOTRCS_D =		$(ROOTETC)/rcS.d
ROOTRC1_D =		$(ROOTETC)/rc1.d
ROOTRC2_D =		$(ROOTETC)/rc2.d

OTHERPROG =     Systems
 
ROOTOTHERPROG = $(OTHERPROG:%=$(ROOTETCLP)/%)
 
TXTS =          $(OTHERPROG)
 
$(ROOTLIBLPPROG) :=     GROUP=  tty
$(ROOTOTHERPROG) :=     OWNER = lp
$(ROOTOTHERPROG) :=     GROUP = lp
$(ROOTOTHERPROG) :=     FILEMODE = 664


OWNER =			root
GROUP =			sys
FILEMODE =		0744

PROG =			lp

ROOTINIT_DPROG =	$(ROOTINIT_D)/$(PROG)
ROOTLNKPROGS =		$(ROOTRC0_D)/K39lp	\
			$(ROOTRCS_D)/K39lp	\
			$(ROOTRC1_D)/K39lp	\
			$(ROOTRC2_D)/S80lp


.KEEP_STATE:

all :			$(PROG)

install:		all $(ROOTINIT_DPROG) $(ROOTETCLP) $(ROOTLNKPROGS) $(ROOTOTHERPROG)

$(ROOTLNKPROGS) :	$(ROOTINIT_DPROG)
			$(RM) $@; $(LN) $(ROOTINIT_DPROG) $@

$(ROOTETCLP):
	$(INS.dir)

$(ROOTETCLP)/% $(ROOTINIT_D)/% :	%
			$(INS.file)

clean clobber strip lint:
