#
#ident	"@(#)Makefile.com	1.6	98/08/28 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.

include		$(SRC)/Makefile.master

LINTLOG=	../lint.$(MACH).log

PKGARCHIVE=	.
DATAFILES=	copyright prototype_com prototype_$(MACH) preinstall \
		postremove depend
README=		SUNWonld-README
FILES=		$(DATAFILES) pkginfo
PACKAGE= 	SUNWonld
ROOTONLD=	$(ROOT)/opt/SUNWonld
ROOTREADME=	$(README:%=$(ROOTONLD)/%)

CLEANFILES=	$(FILES) awk_pkginfo ../bld_awk_pkginfo $(LINTLOG)
CLOBBERFILES=	$(PACKAGE) $(LINTLOG).bak

../%:		../common/%.ksh
		$(RM) $@
		cp $< $@
		chmod +x $@

$(ROOTONLD)/%:	../common/%
		$(RM) $@
		cp $< $@
		chmod +x $@
