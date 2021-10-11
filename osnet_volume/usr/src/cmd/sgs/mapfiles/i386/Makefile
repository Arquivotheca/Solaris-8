#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.3	99/03/30 SMI"
#
include		$(SRC)/Makefile.master


ROOTDIR=	$(ROOT)/usr/lib/ld

DIRS=		$(ROOTDIR)
FILES=		$(ROOTDIR)/map.default


install: $(DIRS) $(FILES)

$(FILES):=	FILEMODE=444
$(FILES):=	GROUP=bin

$(DIRS):=	DIRMODE=755
$(DIRS):=	OWNER=root
$(DIRS):=	GROUP=bin


$(DIRS):
	$(INS.dir)

$(ROOTDIR)/%: %
	$(INS.file)
