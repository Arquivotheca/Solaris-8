#
#ident	"@(#)Makefile	1.1	93/08/13 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

FSTYPE=		cachefs
LIBPROG=	cfstagchk
ATTMK=		$(LIBPROG)

include		../../Makefile.fstype

PROGOBJS=	cfstagchk.o

include		../Makefile.cachefs

LDLIBS		+= -ladm
