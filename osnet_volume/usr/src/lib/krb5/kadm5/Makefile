#
#ident	"@(#)Makefile	1.1	99/07/18 SMI"
#
# Copyright (c) 1995-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/Makefile
#
# include global definitions
include ../../../Makefile.master

#
# Build everything in parallel; use .WAIT for dependencies
.PARALLEL:

COMMON_SUBDIRS= clnt

SUBDIRS= $(COMMON_SUBDIRS)

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
_msg :=		TARGET= _msg

.KEEP_STATE:

all clean clobber: $(SUBDIRS)

_msg: $(SUBDIRS)

install: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
