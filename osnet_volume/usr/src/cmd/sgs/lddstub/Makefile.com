#
#ident	"@(#)Makefile.com	1.1	94/05/18 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
#

PROG=		lddstub
SRCS=		lddstub.s
OBJS=		$(SRCS:%.s=%.o)

include		../../../Makefile.cmd

ASFLAGS=	-P -D_ASM
LDFLAGS=	-dy -e stub $(LDFLAGS.cmd)
