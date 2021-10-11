#
#ident	"@(#)Makefile	1.12	92/08/26 SMI"	/* SVr4 1.11 */
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/fs.d/nfs/exportfs/Makefile

PROG=		exportfs
SRCS=		exportfs.sh

include		../../../Makefile.cmd

all:	$(PROG)

install: all $(ROOTUSRSBINPROG)

clean clobber:
	$(RM) $(PROG)
