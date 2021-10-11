#
# Copyright (c) 1992-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.8	99/10/10 SMI"
#
# lib/cfgadm_plugins/Makefile
#

COMMON_SUBDIRS= scsi
sparc_SUBDIRS=	ac sysctrl

# currently pci library is built only for i386; it should be moved
# to COMMON_SUBDIRS when it is to be built for SPARC also.

i386_SUBDIRS= pci

SUBDIRS= $(COMMON_SUBDIRS) $($(MACH)_SUBDIRS)

ALL_SUBDIRS= $(COMMON_SUBDIRS) $(sparc_SUBDIRS) $(i386_SUBDIRS)

MSGSUBDIRS= $(ALL_SUBDIRS)

all:= 		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint
_msg:=		TARGET= _msg

.KEEP_STATE:

all clean clobber lint: $(SUBDIRS)

install: all $(SUBDIRS)

_msg: $(MSGSUBDIRS)

$(ALL_SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
