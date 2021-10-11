#
#ident	"@(#)Makefile	1.2	92/12/16 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

LIBRARY= ../libt450.a

OBJECTS = arc.o box.o circle.o close.o dot.o erase.o label.o \
        line.o linmod.o move.o open.o point.o space.o subr.o

# include library definitions
include ../../../../lib/Makefile.lib

BUILD.AR = $(AR) $(ARFLAGS) $@ `$(LORDER) $(AROBJS) | $(TSORT)`


CLOBBERFILES= $(LIBRARY)

.KEEP_STATE:

all: $(LIBS)

include ../../../../lib/Makefile.targ

