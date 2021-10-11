#
#pragma ident	"@(#)Makefile	1.9	99/01/25 SMI"
#
# Copyright (c) 1995,1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libsys/sparc/Makefile

MACHOBJ=	libsyss.o
MAPDIR=		../spec/sparc
include		../Makefile.com

.KEEP_STATE:

all:		$(DYNLIB)

install:	all $(ROOTDYNLIB) $(ROOTLINKS)

$(COMSRC):	../common/libsys.sh libsys.list
		sh ../common/libsys.sh

$(MACHSRC):	libsys.sh libsyss.list
		sh ./libsys.sh

include		../Makefile.targ
