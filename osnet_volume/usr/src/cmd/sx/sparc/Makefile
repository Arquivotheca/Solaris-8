#
# Copyright (c) 1993, by Sun Microsystems, Inc.
#
# Makefile definitions for SX-related items
#
#ident	"@(#)Makefile	1.2	95/01/04 SMI"
#
# cmd/sx/sparc/Makefile
#
SUBDIRS	=	sun4m

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
