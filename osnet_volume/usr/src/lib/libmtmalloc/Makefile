#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.2	99/01/25 SMI"
#
# lib/libmtmalloc/Makefile

include ../Makefile.lib

# definitions for install_h target
HDRS=		mtmalloc.h
ROOTHDRDIR=	$(ROOT)/usr/include
ROOTHDRS=	$(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=	$(HDRS:%.h=common/%.check)

$(ROOTHDRS) :=  FILEMODE= 644

# install rule for install_h target
$(ROOTHDRDIR)/%: common/%
	$(INS.file)

SUBDIRS=	spec .WAIT $(MACH)
$(BUILD64)SUBDIRS += $(MACH64)

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
install_h :=	TARGET= install_h

.KEEP_STATE:

all install clean clobber lint: $(SUBDIRS)

install_h: $(ROOTHDRS)

check: $(CHECKHDRS)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
