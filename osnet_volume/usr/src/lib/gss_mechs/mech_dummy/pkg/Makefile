#
#pragma ident	"@(#)Makefile	1.7	99/07/18 SMI"
#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#

# from usr/src/pkgdefs/Makefile.com :

include $(SRC)/Makefile.master

PKGARCHIVE=$(SRC)/../../pkgarchive/$(MACH)/$(RELEASE)
PKGDEFS=$(SRC)/pkgdefs
DATAFILES=copyright
FILES=$(DATAFILES) pkginfo

PACKAGE=gssdummy

CLOBBERFILES= $(FILES)

DATAFILES += depend

ACTION_SUNWgss=grep SUNWgss depend > /dev/null || \
	( chmod 666 depend; \
	echo "P SUNWgss	gss" >> depend; \
	chmod 444 depend );

.KEEP_STATE:

all: $(FILES) action

install: all gather pkg

gather:
	sh getfiles

# action is a pseudotarget denoting completed work on the depend file
action: depend
	$(ACTION_SUNWgss)
	touch $@

# from usr/src/pkgdefs/Makefile.targ :

pkginfo: pkginfo.tmpl $(SRC)/pkgdefs/awk_pkginfo
	$(RM) $@; nawk -f $(SRC)/pkgdefs/awk_pkginfo $@.tmpl > $@

pkg: $(PKGARCHIVE) FRC
	pkgmk -f prototype_$(MACH) -d $(PKGARCHIVE) -r `basename $(ROOT)` \
		-o $(PACKAGE)

$(PKGARCHIVE):
	[ -d $(PKGARCHIVE) ] || mkdir -p $(PKGARCHIVE)

$(DATAFILES): $(SRC)/pkgdefs/common_files/$$@
	$(RM) $@; cp $(SRC)/pkgdefs/common_files/$@ $@

$(MACHDATAFILES): $(SRC)/pkgdefs/common_files/$$@_$(MACH)
	$(RM) $@; cp $(SRC)/pkgdefs/common_files/$@_$(MACH) $@

clobber clean:
	-$(RM) $(CLOBBERFILES) $(CLEANFILES)

FRC:
