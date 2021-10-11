#
#pragma ident	"@(#)Makefile	1.5	99/01/25 SMI"
#
# Copyright (c) 1995,1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libsys/i386/Makefile

MAPDIR=		../spec/i386
include		../Makefile.com

.KEEP_STATE:

all:		$(DYNLIB)

install:	all $(ROOTDYNLIB) $(ROOTLINKS)

$(COMSRC):	../common/libsys.sh libsys.list
		sh ../common/libsys.sh

include		../Makefile.targ
