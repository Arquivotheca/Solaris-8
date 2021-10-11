#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.3	99/03/23 SMI"
#
# lib/libproc/sparcv9/Makefile

MAPDIR=	../spec/sparcv9
include ../Makefile.com
include ../../Makefile.lib.64

.KEEP_STATE:

CPPFLAGS += -D_SYSCALL32

install: all $(ROOTLIBS64) $(ROOTLINKS64) $(ROOTLINT64)
