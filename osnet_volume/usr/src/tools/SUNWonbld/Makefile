#
# Copyright (c) 1993-1998 by Sun Microsystems, Inc.
# All rights reserved.
# 
#ident	"@(#)Makefile	1.2	00/09/14 SMI"
#
include $(SRC)/Makefile.master
#
PACKAGE:sh= basename `pwd`
FILES=$(DATAFILES) pkginfo
DATAFILES=copyright
CLOBBERFILES= $(FILES)
PKGDEFS=$(SRC)/pkgdefs

.KEEP_STATE:

all: $(FILES)

install: all .WAIT pkg

pkginfo: pkginfo.tmpl $(PKGDEFS)/awk_pkginfo
	$(RM) $@; nawk -f $(PKGDEFS)/awk_pkginfo $@.tmpl > $@

pkg: $(PKGARCHIVE) FRC
	pkgmk -f prototype_$(MACH) -d $(PKGARCHIVE) -r $(ROOT) -o $(PACKAGE)

$(PKGARCHIVE):
	[ -d $(PKGARCHIVE) ] || mkdir -p $(PKGARCHIVE)

$(DATAFILES): $(PKGDEFS)/common_files/$$@
	$(RM) $@; cp $(PKGDEFS)/common_files/$@ $@

$(MACHDATAFILES): $(PKGDEFS)/common_files/$$@_$(MACH)
	$(RM) $@; cp $(PKGDEFS)/common_files/$@_$(MACH) $@

clobber clean:
	-$(RM) $(CLOBBERFILES) $(CLEANFILES)


FRC:
