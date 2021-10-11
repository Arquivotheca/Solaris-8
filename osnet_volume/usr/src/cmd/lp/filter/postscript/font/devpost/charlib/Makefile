#
# ident	"@(#)Makefile	1.5	94/01/10 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/lp/filter/postscript/font/devpost/charlib/Makefile
#

include			../../../../../Makefile.lp

CHARFILES =		12 14 34 Fi Fl L1 LH Lb		\
			OLD_LH Sl bx ci ff lc lf lh	\
			ob rc rf rh sq
MAPFILES =		L1.map LH.map Lb.map OLD_LH.map
TXTS =			BRACKETS_NOTE README
FILES =			$(CHARFILES) $(MAPFILES) $(TXTS)

ROOTCHARLIB =		$(ROOT)/usr/lib/font/devpost/charlib

ROOTFILES =		$(FILES:%=$(ROOTCHARLIB)/%)

FILEMODE =		0444

.KEEP_STATE:

all:			$(FILES)

# because of its name, the file ~= is a special case
#
install:		all $(ROOTCHARLIB) $(ROOTFILES)
			$(RM) $(ROOTCHARLIB)/~=
			$(INS) -m $(FILEMODE) -f $(ROOTCHARLIB) ~=
			$(CH)$(CHOWN) $(OWNER) $(ROOTCHARLIB)/~=
			$(CH)$(CHGRP) $(GROUP) $(ROOTCHARLIB)/~=

$(ROOTCHARLIB):
			$(INS.dir)

$(ROOTCHARLIB)/% :	%
			$(INS.file)

clean clobber strip lint :

FRC:
