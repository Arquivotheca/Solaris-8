#
# Copyright (c) 1996-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/01/11 SMI"
#

include		../Makefile.tools

all:=           TARGET= all
install:=       TARGET= install
clean:=         TARGET= clean
clobber:=       TARGET= clobber
lint:=          TARGET= lint

SUBDIRS_sparc	= sparc

# build only on archictures that have a SUBDIRS_arch entry above
SUBDIRS		= $(SUBDIRS_$(MACH))

.KEEP_STATE:

all:		$(SUBDIRS)
install:	$(SUBDIRS)
clean:		$(SUBDIRS)
clobber:	$(SUBDIRS)
lint:		$(SUBDIRS)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
