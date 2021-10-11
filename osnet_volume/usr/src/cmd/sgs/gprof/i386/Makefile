#
#ident	"@(#)Makefile	1.6	97/07/28 SMI"
#
# Copyright (c) 1993,1997 by Sun Microsystems, Inc.
#
# cmd/sgs/gprof/i386/Makefile
#

PROG=		gprof

include 	../Makefile.com

LDLIBS	+=	-lelf

.KEEP_STATE:

all:		$(PROG)

install:	all .WAIT $(ROOTCCSBINPROG) $(ROOTCCSBLURB)

include		../Makefile.targ
