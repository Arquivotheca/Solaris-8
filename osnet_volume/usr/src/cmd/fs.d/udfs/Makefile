#
#ident	"@(#)Makefile	1.5	99/02/13 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#

SUBDIRS=	fsck fsdb fstyp ident labelit mkfs mount

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint
putback:=	TARGET= putback
catalog:=       TARGET= catalog

# for messaging catalog
#
POFILE= udfs.po
POFILES= $(SUBDIRS:%=%/%.po)

.KEEP_STATE:

.PARALLEL:	$(SUBDIRS)

all install:	$(SUBDIRS)

catalog:        $(POFILE)

$(POFILE):      $(SUBDIRS)
	$(RM) 	$@
	cat     $(POFILES)      > $@

clean clobber lint putback : $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(MFLAGS) $(TARGET)

FRC:
