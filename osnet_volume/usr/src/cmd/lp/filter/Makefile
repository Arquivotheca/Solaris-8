#
# ident	"@(#)Makefile	1.5	92/12/15 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/lp/filter/Makefile
#

include 		../Makefile.lp

ROOTLIBLPBIN =		$(ROOTLIBLP)/bin

SUBDIRS =		postscript


PROG =			slow.filter

ROOTLIBLPBINPROG =	$(PROG:%=$(ROOTLIBLPBIN)/%)

.KEEP_STATE :

all :			$(PROG) $(SUBDIRS)

install :		$(PROG) $(ROOTLIBLPBINPROG) $(SUBDIRS)

$(ROOTLIBLPBIN)/% :	%
			$(INS.file)

clean clobber strip lint :	$(SUBDIRS)

$(SUBDIRS) :		FRC
			@cd $@; pwd; $(MAKE) $(TARGET)

FRC :

