#
#ident	"@(#)Makefile	1.13	97/08/31 SMI"
#
# Copyright (c) 1993,1997 by Sun Microsystems, Inc.
#
# cmd/sgs/gprof/sparc/Makefile
#

PROG=		gprof

include 	../Makefile.com

DEFLIST +=	-DSPARC

# cannot use -v because cafe_dem.o has a so-far unfixable warning.
#CFLAGS	+=	-v

LDLIBS	+=	-lelf

.KEEP_STATE:

all:		$(PROG)

install:	all .WAIT $(ROOTCCSBINPROG) $(ROOTCCSBLURB)

include		../Makefile.targ

