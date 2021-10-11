#
#ident	"@(#)Makefile 1.1	97/12/08 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/sgs/yacc/sparcv9/Makefile
#

# This Makefile is only to produce 64-bit yacc shared library liby.so.1
# and not for building 64-bit yacc itself.
#
include ../Makefile.com
include ../../../../lib/Makefile.lib.64

# no messaging files here
POFILE=

.KEEP_STATE:

all_lib: $(LIBS)

install_lib: all_lib $(ROOTLIBS64) $(ROOTLINKS64)

include ../Makefile.targ
include ../../../Makefile.targ

all:	FRC

install:	FRC

FRC:
