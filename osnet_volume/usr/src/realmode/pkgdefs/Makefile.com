#
#ident	"@(#)Makefile.com	1.1	99/06/06 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#

include $(SRC)/Makefile.master

PKGARCHIVE=$(SRC)/../../pkgarchive
PKGDEFS=$(SRC)/pkgdefs
DATAFILES=copyright
FILES=$(DATAFILES) pkginfo

PACKAGE:sh= basename `pwd`

CLOBBERFILES= $(FILES)
