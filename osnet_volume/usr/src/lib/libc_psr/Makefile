#
#ident	"@(#)Makefile	1.5	99/01/25 SMI"
#
# Copyright (c) 1994,1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libc_psr/Makefile
#

include ../Makefile.lib

SUBDIRS=	$(MACH)
SUBDIRS64=	$(MACH64)

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint

.KEEP_STATE:

all install clean clobber lint : lib32 \
				$(BUILD64) lib64

lib32:	spec .WAIT $(SUBDIRS)

lib64:	$(SUBDIRS64)

spec $(SUBDIRS) $(SUBDIRS64): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
