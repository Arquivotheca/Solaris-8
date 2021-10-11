#
#pragma ident	"@(#)Makefile	1.4	99/05/04 SMI"
#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.

include		$(SRC)/cmd/sgs/librtld/Makefile.com

CPPFLAGS += -I../../../../uts/intel/ia32/krtld


.KEEP_STATE:

all:		$(DYNLIB) $(LIBLINKS)

install:	all $(ROOTDYNLIB)

lint:		$(LINTLIB) $(LINTOUT32) $(SGSLINTOUT)

include		$(SRC)/cmd/sgs/librtld/Makefile.targ
