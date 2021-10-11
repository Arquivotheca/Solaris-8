#
#ident	"@(#)Makefile	1.2	96/04/04 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libnull/Makefile
#

LIBRARY= $(LIBNULL)

OBJECTS= __i_am_empty.o

# include library definitions
include ../Makefile.lib

ROOTLIBDIR=	$(ROOT)/usr/lib

# definitions for install_h target

CPPFLAGS= $(CPPFLAGS.master)

.KEEP_STATE:

all: $(TXTS) $(LIBS)

install: $(ROOTLIBS)

install_h:

check:

# include library targets
include ../Makefile.targ
