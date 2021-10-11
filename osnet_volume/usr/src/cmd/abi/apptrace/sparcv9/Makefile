#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.1	99/05/14 SMI"
#

include		../Makefile.com

$(ABILIBS):=	CPICFLAG64= -K pic

.KEEP_STATE:

all:		$(OBJDIR) $(PICDIR) .WAIT $(ABILIBS)

install:	all $(ROOTLIBABI64) .WAIT $(ROOTLIBABILIBS64)

include		../Makefile.targ
include		../../Makefile.sub.64
