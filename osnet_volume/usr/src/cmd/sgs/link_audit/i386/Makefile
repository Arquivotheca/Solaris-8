#
#ident	"@(#)Makefile	1.5	97/11/21 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#

include		../Makefile.com

$(ONLIBS) \
$(CCSLIBS):=	CFLAGS +=	-Kpic

.KEEP_STATE:

all:		$(OBJDIR) $(PICDIR) .WAIT $(CCSLIBS) $(CCSSCRIPTS) \
		$(ONSCRIPTS) $(ONPROGS) $(ONLIBS)

install:	all $(ROOTCCSLIB) .WAIT $(ROOTUSRBINS) $(ROOTCCSLIBS)

install_onld:	install .WAIT $(ROOTONLDLIBS) $(ROOTONLDBINPROG)

include		../Makefile.targ
