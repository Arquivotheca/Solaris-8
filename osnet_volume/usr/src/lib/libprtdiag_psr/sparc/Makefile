#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.2	99/11/09 SMI"
#
# lib/libprtdiag_psr/sparc/Makefile

PRTDIAG_PLATFORMS=	desktop tazmo javelin sunfire starfire

all :=		TARGET= all
lint :=		TARGET= lint
clean :=	TARGET= clean
clobber :=	TARGET= clobber
install :=	TARGET= install

.KEEP_STATE:

all lint clean clobber install : $(PRTDIAG_PLATFORMS)

$(PRTDIAG_PLATFORMS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

