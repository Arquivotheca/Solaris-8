#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.11	99/06/23 SMI"
#
# sgs/libelf/i386/Makefile

MAPDIR=		../spec/i386

#
# Optimization reduced to level O1 due to compiler bug# 4137136
#

include		../Makefile.com

COPTFLAG=	-xO1

SGSMSGTARG +=   $(SGSMSG32)

.KEEP_STATE:

include		../Makefile.targ
