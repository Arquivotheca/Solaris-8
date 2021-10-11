#
#ident	"@(#)Makefile	1.7	92/12/16 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# lib/nametoaddr/npack
#
LIBRARY= libnpack.a

OBJECTS= npack.o

# include library definitions
include ../../Makefile.lib

# set exclusively to avoid libtcpip.so being built up.
# do not change ordering of includes and DYNLIB
DYNLIB= npack.so

CPPFLAGS+=
LINTFLAGS=
DYNFLAGS += -ztext -h $(DYNLIB)
LDLIBS = -lc

LIBS += $(DYNLIB)

.KEEP_STATE:

all: $(LIBS)

install: all $(ROOTLIBS)

# include library targets
include ../../Makefile.targ
