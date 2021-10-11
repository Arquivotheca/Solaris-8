#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.3	99/03/30 SMI"
#
include		$(SRC)/Makefile.master


ROOTDIR=	$(ROOT)/usr/lib/ld
ROOTDIR64=	$(ROOT)/usr/lib/ld/$(MACH64)

DIRS=		$(ROOTDIR) $(ROOTDIR64)
FILES=		$(ROOTDIR64)/map.above4G $(ROOTDIR64)/map.below4G


install: $(DIRS) $(FILES) map.link

$(FILES):=	FILEMODE=444
$(FILES):=	GROUP=bin

$(DIRS):=	DIRMODE=755
$(DIRS):=	GROUP=bin


map.link:
	$(RM) $(ROOTDIR64)/map.default
	$(SYMLINK) ./map.above4G $(ROOTDIR64)/map.default

$(DIRS):
	$(INS.dir)

$(ROOTDIR)/%: %
	$(INS.file)

$(ROOTDIR64)/%: %
	$(INS.file)
