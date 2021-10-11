#
#ident	"@(#)Makefile	1.1	99/09/09 SMI"
#
# Copyright (c) 1994,1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libprtdiag/Makefile
#

include ../Makefile.lib

SUBDIRS=	$(MACH)

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint

.KEEP_STATE:

all install clean clobber lint : $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
