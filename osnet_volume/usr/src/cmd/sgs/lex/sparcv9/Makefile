#
#ident	"@(#)Makefile 1.1	97/12/08 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/sgs/lex/sparcv9/Makefile
#

# This Makefile is only to produce 64-bit lex shared library libl.so.1
# and not for building 64-bit lex itself.
#
include ../Makefile.com
include ../../../../lib/Makefile.lib.64

.KEEP_STATE:

all_lib: $(LIBS)

install_lib:        all_lib $(ROOTLIBS64) $(ROOTLINKS64)

include		../Makefile.targ
include		../../../Makefile.targ

all install:

