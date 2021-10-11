#
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# lib/libdl_stubs
#
LIBRARY= libdl_stubs.a

OBJECTS= dl_stubs.o

# include library definitions
include ../Makefile.lib

CPPFLAGS += -DNSL

LINTFLAGS=

.KEEP_STATE:

all: $(TXTS) $(LIBS)

install: all $(ROOTLIBS)

# include library targets
include ../Makefile.targ
