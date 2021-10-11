#
# Copyright (c) 1993, by Sun Microsystems, Inc.
#
# Makefile definitions for SX-related items
#
#ident	"@(#)Makefile	1.9	93/05/14 SMI"
#
# cmd/sx/Makefile
#
# These utilities are specific to the SPARCstation-10SX platform at this
# time.
#
SUBDIRS	=	sparc

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
cstyle :=	TARGET= cstyle

.KEEP_STATE:

all install cstyle lint clean clobber: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
