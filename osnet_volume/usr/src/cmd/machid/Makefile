#
#ident	"@(#)Makefile	1.2	95/04/21 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG=	machid

include ../Makefile.cmd

#
#	Installation rules to allow an arbitrary rename without triggering
#	repeated attempts at creating the intermedate target.
#
INS.prog= $(RM) $@; $(INS) -s -m $(FILEMODE) -f $(@D) $(PROG)
$(CH)INS.prog = \
	$(RM) $@; \
	$(INS) -s -m $(FILEMODE) -u $(OWNER) -g $(GROUP) -f $(@D) $(PROG)

#
#	List of all links present on all architectures and machines.
#
#	Note that this function is obsolesent and we don't generally
#	add to this list (see psarc/1992/171).
#
FIRSTLINK =	i286
LINKS =		i386 i486 i860 i86pc iAPX286 \
		m68k mc68000 mc68010 mc68020 mc68030 mc68040 \
		sparc sun sun2 sun3 sun3x sun4 sun4c sun4m sun4d sun4e \
		u370 u3b u3b15 u3b2 u3b5 vax pdp11

ROOTFIRSTLINK =	$(ROOTBIN)/$(FIRSTLINK)
ROOTLINKS =	$(LINKS:%=$(ROOTBIN)/%)

#
#	Set ownership on links.
#
CHMODLINK=	$(CHMOD) $(FILEMODE) $@
CHOWNLINK=
$(CH)CHOWNLINK=	$(CHOWN) $(OWNER) $@
CHGRPLINK=
$(CH)CHGRPLINK=	$(CHGRP) $(GROUP) $@

#
#     Install the program as the first machine in the list.
#
$(ROOTFIRSTLINK): $(PROG)
	$(INS.prog)
	$(MV) $(@D)/$(PROG) $@

#
#	Link installation rules
#
$(ROOTBIN)/%: $(PROG)
	$(RM) $@
	$(LN) $(ROOTFIRSTLINK) $@
	$(CHMODLINK)
	$(CHOWNLINK)
	$(CHGRPLINK)

CFLAGS += -v
CCMODE=	-Xa

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTFIRSTLINK) $(ROOTLINKS)

clean:

lint:	lint_PROG

include ../Makefile.targ
