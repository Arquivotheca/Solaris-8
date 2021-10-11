#
#ident	"@(#)Makefile	1.3	99/06/11 SMI"
#
# Copyright (c) 1999, by Sun Microsystems, Inc.
# All rights reserved.
#
include $(SRC)/Makefile.master

SLP_COMPONENTS= javalib etc
SUBDIRS= spec .WAIT  $(SLP_COMPONENTS) $(MACH) $(BUILD64) $(MACH64)

# include library definitions
include ../Makefile.lib

HDRS=clib/slp.h clib/slp-internal.h clib/slp_net_utils.h clib/slp_ami.h
ROOTHDRDIR=     $(ROOT)/usr/include
ROOTHDRS=       $(ROOTHDRDIR)/slp.h

CHECKHDRS= $(HDRS:%.h=%.check)

MSGFILE=libslp.po

# install rule for install_h target
$(ROOTHDRDIR)/%: clib/%
	$(INS.file)

#install rule
$(ROOTDIRS)/%: %
	$(INS.file)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
install :=	TARGET= install
lint :=		TARGET= lint
_msg :=		TARGET= _msg

.PARALLEL: $(SUBDIRS)

all lint install: install_h .WAIT $(SUBDIRS)

clean clobber: $(SUBDIRS)

_msg:	$(SLP_COMPONENTS) $(MSGDOMAIN)
	$(RM) $(MSGDOMAIN)/$(MSGFILE)
	cp clib/$(MSGFILE) $(MSGDOMAIN)

# include library targets
include ../Makefile.targ

install_h:	$(ROOTHDRS)

check: $(CHECKHDRS)

$(MACH) $(MACH64) spec $(SLP_COMPONENTS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
