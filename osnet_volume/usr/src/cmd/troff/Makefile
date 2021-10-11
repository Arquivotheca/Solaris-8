#
# ident	"@(#)Makefile	1.8	92/12/15 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/troff/Makefile
#

include ../Makefile.cmd

SUBDIRS =	nroff.d troff.d


all :=		TARGET = all
install :=	TARGET = install
clean :=	TARGET = clean
clobber :=	TARGET = clobber
lint :=		TARGET = lint
strip :=	TARGET = strip
_msg :=         TARGET = catalog

#
# for message catalog
#
POFILES= $(SUBDIRS:%=%/%.po)
POFILE= troff.po


.KEEP_STATE :

all :		$(SUBDIRS)

install clean lint strip :	$(SUBDIRS)

clobber: $(SUBDIRS) local_clobber

local_clobber:
	$(RM)	$(CLOBBERFILES)

$(POFILE):      $(SUBDIRS) $(POFILES2)
	cat     $(POFILES) $(POFILES2)  >       $@
	 
_msg:   $(SUBDIRS)
	  $(RM)	$(POFILE)
	  cat    $(POFILES)      > $(POFILE)
	  $(RM) $(MSGDOMAIN)/$(POFILE)
	  cp $(POFILE) $(MSGDOMAIN)

$(SUBDIRS) :	FRC
		@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
