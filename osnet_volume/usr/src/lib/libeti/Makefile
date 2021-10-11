#
#ident	"@(#)Makefile	1.5	93/02/04 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# lib/libeti/Makefile
#
# include global definitions
include ../../Makefile.master

SUBDIRS= form menu panel
HDRSUBDIRS= form menu panel

all:=		TARGET= all
clean:=		TARGET= clean
clobber:=	TARGET= clobber
install:=	TARGET= install
install_h:=	TARGET= install_h
check:=		TARGET= check
lint:=		TARGET= lint

# definitions for install_h target
HDRS=           eti.h
ROOTHDRDIR=     $(ROOT)/usr/include
ROOTHDRS=       $(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=	$(HDRS:%.h=%.check)

# install rule for install_h target
$(ROOTHDRDIR)/%: %
	$(INS.file)

.KEEP_STATE:

all clean clobber install lint: $(SUBDIRS)

install_h: $(ROOTHDRS) $(HDRSUBDIRS)

check:	$(CHECKHDRS) $(HDRSUBDIRS)

xxx:
	@cd form; pwd; $(MAKE) $(TARGET)
	@cd menu; pwd; $(MAKE) $(TARGET)
	@cd panel; pwd; $(MAKE) $(TARGET)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
