#
#ident	"@(#)Makefile	1.6	99/06/23 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#

include		../Makefile.com

SGSMSGTARG +=	$(SGSMSGINTEL)

.KEEP_STATE:

all:		$(DYNLIB) $(LIBLINKS)

install:	all $(ROOTDYNLIB) $(ROOTLINKS) $(ROOTLIBDIR)/$(LINTLIB) \
		$(ROOTLIBDIR)/$(LINTLIBSRC)

include		../Makefile.targ
