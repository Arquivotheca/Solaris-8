#
#ident	"@(#)Makefile	1.4	97/12/08 SMI"
#
# Copyright (c) 1995,1997 by Sun Microsystems, Inc.
# All Rights Reserved.
#
# cmd/sgs/yacc/Makefile
#

include		../../../Makefile.master

SUBDIRS=	$(MACH) $(BUILD64) $(MACH64)

CSRC=		common/y1.c common/y2.c common/y3.c common/y4.c

# conditional statements

all :=		TARGET= all
install :=	TARGET= install
install_lib :=	TARGET= install_lib
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint

# for messaging catalog
#

POFILE= yacc.po

.KEEP_STATE:

all clean clobber install install_lib lint:	$(SUBDIRS)

catalog:	$(POFILE)

$(POFILE):	
	$(RM) $@
	$(XGETTEXT) $(XGETFLAGS) $(CSRC)
	sed "/^domain/d" < messages.po > $@
	$(RM) messages.po

$(MACH) $(MACH64): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
