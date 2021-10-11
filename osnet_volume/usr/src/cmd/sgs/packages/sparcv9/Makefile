#
#ident	"@(#)Makefile	1.1	97/10/27 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#


include ../Makefile.com


.KEEP_STATE:


DATAFILES64=		copyright depend
PACKAGE=		SUNWldv9
FILES64=		$(DATAFILES64) pkginfo
CLEANFILES64=		$(FILES64) awk_pkginfo ../bld_awk_pkginfo
CLOBBERFILES=		$(PACKAGE)


all: $(FILES64)
install: all .WAIT pkg


$(DATAFILES64): ../common/$$@
	$(RM) $@; cp ../common/$@ $@

pkginfo: pkginfo.tmpl awk_pkginfo
	$(RM) $@; nawk -f awk_pkginfo $@.tmpl > $@

pkg: FRC
	@rm -rf $(PACKAGE)
	pkgmk -f prototype -d $(PKGARCHIVE) -r $(ROOT) \
		-o $(PACKAGE) \
		2>&1 | grep -v "$(SRC)"
	pkgtrans -o -s . $(PACKAGE).tmp $(PACKAGE)
	rm -rf $(PACKAGE)
	mv $(PACKAGE).tmp $(PACKAGE)

awk_pkginfo:	../bld_awk_pkginfo
	../bld_awk_pkginfo -m $(MACH) -p "$(RELEASE)/$(VERSION)" -o $@

clean: FRC
	-$(RM) $(CLEANFILES64)

clobber: clean FRC
	-$(RM) -r $(CLOBBERFILES)

FRC:
