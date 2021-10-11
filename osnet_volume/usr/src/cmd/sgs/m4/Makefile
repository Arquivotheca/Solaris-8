#
#ident	"@(#)Makefile	1.2	95/02/02 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All Rights Reserved.
#
# cmd/sgs/m4/Makefile
#

include		../../../Makefile.master

SUBDIRS=	$(MACH)

CSRC=		common/m4.c common/m4macs.c

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint

# for messaging catalog
#
POFILE= m4.po


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
