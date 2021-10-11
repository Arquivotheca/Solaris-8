#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.21	97/09/09 SMI"

PROG =	crash

include ../Makefile.cmd

SUBDIRS =	$(MACH)
$(BUILD64)SUBDIRS += $(MACH64)

ROOTETCLINK =	$(ROOTETC)/$(PROG)
REL_ETC_SBIN =	../usr/sbin

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint

.KEEP_STATE:

all:	$(SUBDIRS)

clean clobber lint:	$(SUBDIRS)

$(ROOTETCLINK):
	$(RM) $@; $(SYMLINK) $(REL_ETC_SBIN)/$(PROG) $@

install:	$(SUBDIRS) $(ROOTETCLINK)
	-$(RM) $(ROOTUSRSBINPROG)
	-$(LN) $(ISAEXEC) $(ROOTUSRSBINPROG)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

include ../Makefile.targ
