#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.1	99/01/25 SMI"
#
# lib/libthread_db/spec/sparc/Makefile

ABIVERS=	.0 .1
MAPFILES=	$(ABIVERS:%=mapfile%)

all		:=	TARGET= all
install		:=	TARGET= install
clean		:=	TARGET= clean
clobber		:=	TARGET= clobber
lint		:=	TARGET= lint

.KEEP_STATE:

all install clean clobber lint: $(ABIVERS)

# spec files do not have msgs or catalogs
_msg catalog:

$(ABIVERS): FRC
	@ $(MAKE) -f Makefile$@ $(TARGET)

mapfile: $(MAPFILES)

mapfile.%: Makefile.% FRC
	@ $(MAKE) -f $< $@

FRC:
