#
#ident	"@(#)Makefile	1.3	97/12/08 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# cmd/sgs/yacc/i386/Makefile
#

PROG=		yacc
DEFLIST=	-DNOLIBW -DELF

include 	../../../Makefile.cmd
include		../Makefile.com

.KEEP_STATE:

all:		$(PROG)

all_lib:	$(LIBS)

install:	all $(ROOTCCSBINPROG) $(ROOTYACCPAR)

# The static library liby.a only gets installed for 32-bit, and is installed
# in /usr/ccs/lib rather than /usr/lib, hence the separate rule. In addition,
# we need to link /usr/ccs/lib/liby.so with /usr/lib/liby.so.1 to accomodate
# the order search of the linker
#
install_lib:	all_lib $(STATICLIB) $(ROOTLIBS) $(ROOTLINKS) $(DYNLINKLIB) $(ROOTLINT)

include		../Makefile.targ
include		../../../Makefile.targ
