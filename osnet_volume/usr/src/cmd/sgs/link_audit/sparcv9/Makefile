#
#ident	"@(#)Makefile	1.5	98/09/23 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#

include		../Makefile.com

$(ONLIBS) \
$(CCSLIBS):=	CFLAGS64 +=	-Kpic -erroff=E_ASM_DISABLES_OPTIMIZATION

LINTFLAGS64 +=	-D__sparcv9

.KEEP_STATE:

all:		$(PICDIR) .WAIT $(CCSLIBS) $(CCSSCRIPTS) \
		$(ONSCRIPTS) $(ONLIBS)

install:	all $(ROOTCCSLIB64) .WAIT $(ROOTCCSLIBS64)

install_onld:	install .WAIT $(ROOTONLDLIBS64)

include		../Makefile.targ
include		../../Makefile.sub.64
