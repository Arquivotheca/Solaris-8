#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.3	99/07/27 SMI"
#
# lib/libdhcputil/Makefile
#

include ../Makefile.lib

SUBDIRS =	$(MACH)

all	:=	TARGET = all
lint	:=	TARGET = lint
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
install	:=	TARGET = install

# definitions for install_h target
HDRS       = dhcp_inittab.h dhcpmsg.h
ROOTHDRDIR = $(ROOT)/usr/include
ROOTHDRS   = $(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS  = $(HDRS:%.h=common/%.check)
$(ROOTHDRS) := FILEMODE = 644

.KEEP_STATE:

# install rule for install_h target
$(ROOTHDRDIR)/%: common/%
	$(INS.file)

all clean clobber lint install: spec .WAIT $(SUBDIRS)

install_h:	$(ROOTHDRS)

check:		$(CHECKHDRS)

_msg:

spec $(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
