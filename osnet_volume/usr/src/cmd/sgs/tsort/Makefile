#
#ident	"@(#)Makefile	1.2	95/02/02 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/sgs/tsort/Makefile
#

include		../../../Makefile.master

SUBDIRS=	$(MACH)

CSRC=		common/errusage.c common/tsort.c

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint

# for messaging catalog
#
POFILE= tsort.po

.KEEP_STATE:

all clean clobber install lint:	$(SUBDIRS)

catalog:	$(POFILE)

$(POFILE):	
	$(RM) $@
	$(XGETTEXT) $(XGETFLAGS) $(CSRC)
	sed "/^domain/d" < messages.po > $@
	$(RM) messages.po

$(SUBDIRS):	FRC
		@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
