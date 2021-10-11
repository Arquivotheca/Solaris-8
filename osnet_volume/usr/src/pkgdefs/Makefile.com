#
#ident	"@(#)Makefile.com	1.3	93/10/01 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#

include $(SRC)/Makefile.master

PKGARCHIVE=$(SRC)/../../pkgarchive
PKGDEFS=$(SRC)/pkgdefs
DATAFILES=copyright
FILES=$(DATAFILES) pkginfo

PACKAGE:sh= basename `pwd`

CLOBBERFILES= $(FILES)
