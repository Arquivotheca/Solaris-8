#
# Copyright (c) 1997,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.19	99/12/06 SMI"
#
# lib/gss_mechs/mech_dummy/Makefile
#
#
# This make file will build mech_dummy.so.1. This shared object
# contains all the functionality needed to support the Dummy GSS-API
# mechanism. 
#

include ../../../Makefile.master

SUBDIRS= $(MACH) $(BUILD64) $(MACH64)

# include library definitions
include ../../Makefile.lib

HDRS=

CHECKHDRS= $(HDRS:%.h=%.check)

$(ROOTDIRS)/%:	%
	$(INS.file)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
install :=	TARGET= install
lint :=		TARGET= lint

.KEEP_STATE:

all:	.WAIT $(SUBDIRS)

lint:	.WAIT $(SUBDIRS)

install: all

install_h:

DUPLICATE_SRC = dmech.c

clean clobber:  $(SUBDIRS)

check: $(CHECKHDRS)

do_pkg:
	cd pkg ; pwd ; $(MAKE) install


# include library targets
# include ../../Makefile.targ

$(MACH) $(MACH64):      FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:



