#
#ident	"@(#)Makefile	1.16	97/09/09 SMI"
#
# Copyright (c) 1989-1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/adb/Makefile
#

PROG=	adb

include ../Makefile.cmd

#
# One for each ISA.  No longer worried about platforms.
#
SUBDIRS=	$(MACH)
$(BUILD64)SUBDIRS += $(MACH64)

TXTS= README common/MakeCommon

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint

.KEEP_STATE:

all:	$(SUBDIRS) $(TXTS)

clean clobber lint:	$(SUBDIRS)

install:	$(SUBDIRS)
	-$(RM) $(ROOTPROG)
	-$(LN) $(ISAEXEC) $(ROOTPROG)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

include ../Makefile.targ
