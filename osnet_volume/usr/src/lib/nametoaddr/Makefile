#
# Copyright (c) 1989 by Sun Microsystems, Inc.
# All rights reserved.
#ident	"@(#)Makefile	1.13	97/08/21 SMI"
#
# lib/nametoaddr/Makefile
#
# include global definitions
include ../../Makefile.master

SUBDIRS= straddr

all:=		TARGET= all
clean:=		TARGET= clean
clobber:=	TARGET= clobber
install:=	TARGET= install
lint:=		TARGET= lint

.KEEP_STATE:

all clean clobber install lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)


FRC:

