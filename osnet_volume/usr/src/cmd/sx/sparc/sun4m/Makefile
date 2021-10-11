#
# Copyright (c) 1993, 1995, 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# Makefile definitions for SX-related items
#
#ident	"@(#)Makefile	1.3	97/12/08 SMI"
#
# cmd/sx/sparc/sun4m/Makefile
#
SUBDIRS	=	cg14config sxconfig

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
